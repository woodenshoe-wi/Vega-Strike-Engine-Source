#include "base.h"
#include "gldrv/winsys.h"
#include "vsfilesystem.h"
#include "lin_time.h"
#include "audiolib.h"
#include "gfx/camera.h"
#include "gfx/cockpit_generic.h"
#include "python/init.h"
#include "planet_generic.h"
#include <Python.h>
#include <algorithm>
#include "base_util.h"
#include "config_xml.h"
#include "save_util.h"
#include "unit_util.h"
#include "gfx/cockpit.h"
#include "gfx/ani_texture.h"
#include "music.h"
#include "lin_time.h"
#include "load_mission.h"
#include "universe_util.h"
#ifdef RENDER_FROM_TEXTURE
#include "gfx/stream_texture.h"
#endif
#include "main_loop.h"
static void biModifyMouseSensitivity(int &x, int &y, bool invert){
  int xrez=g_game.x_resolution;
  int yrez=g_game.y_resolution;
  static int whentodouble=XMLSupport::parse_int(vs_config->getVariable("joystick","double_mouse_position","1280"));
  static float factor=XMLSupport::parse_float(vs_config->getVariable("joystick","double_mouse_factor","2"));
  if (xrez>=whentodouble) {
    x-=g_game.x_resolution/2;
    y-=g_game.y_resolution/2;
    if (invert) {
      x/=factor;
      y/=factor;
    }else {
      x*=factor;
      y*=factor;
    }
    x+=g_game.x_resolution/2;
    y+=g_game.y_resolution/2;
    if (x>g_game.x_resolution)
      x=g_game.x_resolution;
    if (y>g_game.y_resolution)
      y=g_game.y_resolution;
    if (x<0) x=0;
    if (y<0) y=0;
  }
}
static bool createdbase=false;

void ModifyMouseSensitivity(int &x, int &y) {
  biModifyMouseSensitivity(x,y,false);
}
#ifdef BASE_MAKER
 #include <stdio.h>
 #ifdef _WIN32
  #include <windows.h>
 #endif
static char makingstate=0;
#endif
extern const char *mission_key; //defined in main.cpp
bool BaseInterface::Room::BaseTalk::hastalked=false;
using namespace VSFileSystem;
#define NEW_GUI

#ifdef NEW_GUI
#include "basecomputer.h"
#include "../gui/eventmanager.h"
#endif
std::vector<unsigned int>base_keyboard_queue;
static void CalculateRealXAndY (int xbeforecalc, int ybeforecalc, float *x, float *y) {
	(*x)=(((float)(xbeforecalc*2))/g_game.x_resolution)-1;
	(*y)=-(((float)(ybeforecalc*2))/g_game.y_resolution)+1;
}
#define mymin(a,b) (((a)<(b))?(a):(b))
static void SetupViewport() {
        static int base_max_width=XMLSupport::parse_int(vs_config->getVariable("graphics","base_max_width","0"));
        static int base_max_height=XMLSupport::parse_int(vs_config->getVariable("graphics","base_max_height","0"));
        if (base_max_width&&base_max_height) {
          int xrez = mymin(g_game.x_resolution,base_max_width);
          int yrez = mymin(g_game.y_resolution,base_max_height);
          int offsetx = (g_game.x_resolution-xrez)/2;
          int offsety = (g_game.y_resolution-yrez)/2;
          glViewport(offsetx,offsety,xrez,yrez);
        }
}
#undef mymin
BaseInterface::Room::~Room () {
	int i;
	for (i=0;i<links.size();i++) {
		if (links[i])
			delete links[i];
	}
	for (i=0;i<objs.size();i++) {
		if (objs[i])
			delete objs[i];
	}
}

BaseInterface::Room::Room () {
//		Do nothing...
}

void BaseInterface::Room::BaseObj::Draw (BaseInterface *base) {
//		Do nothing...
}

static FILTER BlurBases() {
  static bool blur_bases = XMLSupport::parse_bool(vs_config->getVariable("graphics","blur_bases","true"));
  return blur_bases?BILINEAR:NEAREST;
}
BaseInterface::Room::BaseVSSprite::BaseVSSprite (const char *spritefile, std::string ind) 
  : BaseObj(ind),spr(spritefile,BlurBases(),GFXTRUE) {}

void BaseInterface::Room::BaseVSSprite::Draw (BaseInterface *base) {
  static float AlphaTestingCutoff = XMLSupport::parse_float(vs_config->getVariable("graphics","base_alpha_test_cutoff","0"));  
  GFXAlphaTest (GREATER,AlphaTestingCutoff);
  GFXBlendMode(SRCALPHA,INVSRCALPHA);
  GFXEnable(TEXTURE0);
  spr.Draw();
  GFXAlphaTest (ALWAYS,0);
}

void BaseInterface::Room::BaseShip::Draw (BaseInterface *base) {
	Unit *un=base->caller.GetUnit();
	if (un) {
		GFXHudMode (GFXFALSE);
		Vector p,q,r;
		_Universe->AccessCamera()->GetOrientation (p,q,r);
		int co=_Universe->AccessCamera()->getCockpitOffset();
		_Universe->AccessCamera()->setCockpitOffset(0);
		_Universe->AccessCamera()->UpdateGFX();
		QVector pos =  _Universe->AccessCamera ()->GetPosition();
		Matrix cam (p.i,p.j,p.k,q.i,q.j,q.k,r.i,r.j,r.k,pos);
		Matrix final;
		Matrix newmat = mat;
		newmat.p.k*=un->rSize();
		newmat.p+=QVector(0,0,g_game.znear);
		newmat.p.i*=newmat.p.k;
		newmat.p.j*=newmat.p.k;
		MultMatrix (final,cam,newmat);
                SetupViewport();
		GFXClear(GFXFALSE);//clear the zbuf

		GFXEnable (DEPTHTEST);
		GFXEnable (DEPTHWRITE);
		GFXEnable(LIGHTING);
		int light=0;
		GFXCreateLight(light,GFXLight(true,GFXColor(1,1,1,1),GFXColor(1,1,1,1),GFXColor(1,1,1,1),GFXColor(.1,.1,.1,1),GFXColor(1,0,0),GFXColor(1,1,1,0),24),true);

		(un)->DrawNow(final,FLT_MAX);
		GFXDeleteLight(light);
		GFXDisable (DEPTHTEST);
		GFXDisable (DEPTHWRITE);
		GFXDisable(LIGHTING);
                GFXDisable(TEXTURE1);
                GFXEnable(TEXTURE0);
		_Universe->AccessCamera()->setCockpitOffset(co);
		_Universe->AccessCamera()->UpdateGFX();
                SetupViewport();                
//		_Universe->AccessCockpit()->SetView (CP_PAN);
		GFXHudMode (GFXTRUE);
	}
}

void BaseInterface::Room::Draw (BaseInterface *base) {
	int i;
	for (i=0;i<objs.size();i++) {
		if (objs[i]) {
			GFXBlendMode(SRCALPHA,INVSRCALPHA);
			objs[i]->Draw(base);
			
		}
	}
	GFXBlendMode(SRCALPHA,INVSRCALPHA);
	// draw location markers
	//<!-- config options in the "graphics" section -->
	//<var name="base_enable_locationmarkers" value="true"/>
	//<var name="base_locationmarker_sprite" value="base_locationmarker.spr"/>
	//<var name="base_draw_locationtext" value="true"/>
	//<var name="base_locationmarker_textoffset_x" value="0.025"/>
	//<var name="base_locationmarker_textoffset_y" value="0.025"/>
	//<var name="base_locationmarker_drawalways" value="false"/>
	//<var name="base_locationmarker_distance" value="0.5"/>
	//<var name="base_locationmarker_textcolor_r" value="1.0"/>
	//<var name="base_locationmarker_textcolor_g" value="1.0"/>
	//<var name="base_locationmarker_textcolor_b" value="1.0"/>
	//<var name="base_drawlocationborders" value="false"/>
	static bool enable_markers = XMLSupport::parse_bool(vs_config->getVariable("graphics","base_enable_locationmarkers","false"));
	static bool draw_text      = XMLSupport::parse_bool(vs_config->getVariable("graphics","base_draw_locationtext","false"));
	static bool draw_always    = XMLSupport::parse_bool(vs_config->getVariable("graphics","base_locationmarker_drawalways","false"));
	static float y_lower       = -0.9; // shows the offset on the lower edge of the screen (for the textline there) -> Should be defined globally somewhere
	if (enable_markers) {
		float x, y, text_wid, text_hei;
		for (int i=0; i<links.size(); i++) { //loop through all links and draw a marker for each
			if (links[i]) {
			if ((links[i]->alpha < 1) || (draw_always)) {
				if (draw_always) { links[i]->alpha = 1; }  // set all alphas to visible
				x = (links[i]->x + (links[i]->wid / 2));   // get the center of the location
				y = (links[i]->y + (links[i]->hei / 2));   // get the center of the location

				/* draw marker */
				static string spritefile_marker = vs_config->getVariable("graphics","base_locationmarker_sprite","");
				if (spritefile_marker.length()) {
					static VSSprite *spr_marker = new VSSprite(spritefile_marker.c_str());
					float wid,hei;
					spr_marker->GetSize(wid,hei);
					// check if the sprite is near a screenedge and correct its position if necessary
					if ((x + (wid / 2)) >=  1     ) {x = ( 1      - (wid / 2));}
					if ((y + (hei / 2)) >=  1     ) {y = ( 1      - (hei / 2));}
					if ((x - (wid / 2)) <= -1     ) {x = (-1      + (wid / 2));}
					if ((y - (hei / 2)) <= y_lower) {y = (y_lower + (hei / 2));}
					spr_marker->SetPosition(x, y);
					GFXDisable(TEXTURE1);
					GFXEnable(TEXTURE0);
					GFXColor4f(1,1,1,links[i]->alpha);
					spr_marker->Draw();
				} // if spritefile

				if (draw_text) {
					GFXDisable(TEXTURE0);
					//get offset from config;
					static float text_offset_x = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_textoffset_x","0"));
					static float text_offset_y = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_textoffset_y","0"));
					static float text_color_r  = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_textcolor_r","1"));
					static float text_color_g  = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_textcolor_g","1"));
					static float text_color_b  = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_textcolor_b","1"));
					TextPlane text_marker;
					text_marker.SetText(links[i]->text);
					text_marker.GetCharSize(text_wid, text_hei);     // get average charactersize
					float text_pos_x = x + text_offset_x;            // align right ...
					float text_pos_y = y + text_offset_y + text_hei; // ...and on top
					text_wid = text_wid * links[i]->text.length() * 0.25;     // calc ~width of text (=multiply the average characterwidth with the number of characters)
					if ((text_pos_x + text_offset_x + text_wid) >= 1) {       // check right screenborder
						text_pos_x = (x - abs(text_offset_x) - text_wid);     // align left
					}
					if ((text_pos_y + text_offset_y) >= 1) {                  // check upper screenborder
						text_pos_y = (y - abs(text_offset_y));                // align on bottom
					}
					if ((text_pos_y + text_offset_y - text_hei) <= y_lower) { // check lower screenborder
						text_pos_y = (y + abs(text_offset_y) + text_hei);     // align on top
					}
					text_marker.col = GFXColor(text_color_r, text_color_g, text_color_b, links[i]->alpha);
					text_marker.SetPos(text_pos_x, text_pos_y);
					text_marker.Draw();
					GFXEnable(TEXTURE0);
				} // if draw_text
			}
			} // if link
		}  // for i
		static bool draw_borders = XMLSupport::parse_bool(vs_config->getVariable("graphics","base_drawlocationborders","false"));
		if (draw_borders) {
			for (int i=0; i<links.size(); i++) { //loop through all links and draw a marker for each
				if (links[i]) {
					GFXColor4f(1,1,1,1);
					GFXDisable(TEXTURE0);
					GFXBegin (GFXLINE);
					GFXVertex3d (links[i]->x              ,links[i]->y              ,0.0);
					GFXVertex3d (links[i]->x              ,links[i]->y+links[i]->hei,0.0);

					GFXVertex3d (links[i]->x              ,links[i]->y+links[i]->hei,0.0);
					GFXVertex3d (links[i]->x+links[i]->wid,links[i]->y+links[i]->hei,0.0);

					GFXVertex3d (links[i]->x+links[i]->wid,links[i]->y+links[i]->hei,0.0);
 					GFXVertex3d (links[i]->x+links[i]->wid,links[i]->y              ,0.0);

					GFXVertex3d (links[i]->x+links[i]->wid,links[i]->y              ,0.0);
					GFXVertex3d (links[i]->x              ,links[i]->y              ,0.0);
					GFXEnd();
					GFXEnable (TEXTURE0);
				} // if link
			} // for i
		} // if draw_borders
	} // enable_markers

}
static std::vector<BaseInterface::Room::BaseTalk *> active_talks;

BaseInterface::Room::BaseTalk::BaseTalk (std::string msg,std::string ind, bool only_one) :BaseObj(ind), curchar (0), curtime (0), message(msg) {
	if (only_one) {
		active_talks.clear();
	}
	active_talks.push_back(this);
}

void BaseInterface::Room::BaseText::Draw (BaseInterface *base) {
  int tmpx=g_game.x_resolution;
  int tmpy=g_game.y_resolution;
  static int base_max_width=XMLSupport::parse_int(vs_config->getVariable("graphics","base_max_width","0"));
  static int base_max_height=XMLSupport::parse_int(vs_config->getVariable("graphics","base_max_height","0"));
  if (base_max_width&&base_max_height) {
    if (base_max_width<tmpx)
      g_game.x_resolution=base_max_width;
    if (base_max_height<tmpy)
      g_game.y_resolution=base_max_height;
  }
  text.Draw();
  g_game.x_resolution=tmpx;
  g_game.y_resolution=tmpy;
}

void RunPython(const char *filnam) {
	printf("Run python:\n%s\n", filnam);
	if (filnam[0]) {
		if (filnam[0]=='#') {
			::Python::reseterrors();
			PyRun_SimpleString(const_cast<char*>(filnam));
			::Python::reseterrors();
		}else {
			FILE *fp=VSFileSystem::vs_open(filnam,"r");
			if (fp) {
				int length=strlen(filnam);
				char *newfile=new char[length+1];
				strncpy(newfile,filnam,length);
				newfile[length]='\0';
				::Python::reseterrors();
				PyRun_SimpleFile(fp,newfile);
				::Python::reseterrors();
				fclose(fp);
				processDelayedMissions();
			} else {
				fprintf(stderr,"Warning:python link file '%s' not found\n",filnam);
			}
		}
	}
}

void BaseInterface::Room::BasePython::Draw (BaseInterface *base) {
	timeleft+=GetElapsedTime()/getTimeCompression();
	if (timeleft>=maxtime) {
		timeleft=0;
		printf("Running python script... ");
		RunPython(this->pythonfile.c_str());
		return; //do not do ANYTHING with 'this' after the previous statement...
	}
}

void BaseInterface::Room::BaseTalk::Draw (BaseInterface *base) {
/*	GFXColor4f(1,1,1,1);
	GFXBegin(GFXLINESTRIP);
		GFXVertex3f(caller->x,caller->y,0);
		GFXVertex3f(caller->x+caller->wid,caller->y,0);
		GFXVertex3f(caller->x+caller->wid,caller->y+caller->hei,0);
		GFXVertex3f(caller->x,caller->y+caller->hei,0);
		GFXVertex3f(caller->x,caller->y,0);
	GFXEnd();*/
	
	// FIXME: should be called from draw()
	if (hastalked) return;
	curtime+=GetElapsedTime()/getTimeCompression();
	static float delay=XMLSupport::parse_float(vs_config->getVariable("graphics","text_delay",".05"));
	if ((std::find(active_talks.begin(),active_talks.end(),this)==active_talks.end())||(curchar>=message.size()&&curtime>((delay*message.size())+2))) {
		curtime=0;
		BaseObj * self=this;
		std::vector<BaseObj *>::iterator ind=std::find(base->rooms[base->curroom]->objs.begin(),
				base->rooms[base->curroom]->objs.end(),
				this);
		if (ind!=base->rooms[base->curroom]->objs.end()) {
			*ind=NULL;
		}
		std::vector<BaseTalk *>::iterator ind2=std::find(active_talks.begin(),active_talks.end(),this);
		if (ind2!=active_talks.end()) {
			*ind2=NULL;
		}
		base->othtext.SetText("");
		delete this;
		return; //do not do ANYTHING with 'this' after the previous statement...
	}
	if (curchar<message.size()) {
		static float inbetween=XMLSupport::parse_float(vs_config->getVariable("graphics","text_speed",".025"));
		if (curtime>inbetween) {
			base->othtext.SetText(message.substr(0,++curchar));
			curtime=0;
		}
	}
	hastalked=true;
}

int BaseInterface::Room::MouseOver (BaseInterface *base,float x, float y) {
	for (int i=0;i<links.size();i++) {
		if (links[i]) {
			if (x>=links[i]->x&&
					x<=(links[i]->x+links[i]->wid)&&
					y>=links[i]->y&&
					y<=(links[i]->y+links[i]->hei)) {
				return i;
			}
		}
	}
	return -1;
}

BaseInterface *BaseInterface::CurrentBase=NULL;
static BaseInterface *lastBaseDoNotDereference=NULL;

bool RefreshGUI(void) {
	bool retval=false;
	if (_Universe->AccessCockpit()) {
		if (BaseInterface::CurrentBase) {
			if (_Universe->AccessCockpit()->GetParent()==BaseInterface::CurrentBase->caller.GetUnit()){
				if (BaseInterface::CurrentBase->CallComp) {
#ifdef NEW_GUI
					globalWindowManager().draw();
					return true;
#else
					return RefreshInterface ();
#endif
				} else {
					BaseInterface::CurrentBase->Draw();
				}
				retval=true;
			}
		}
	}
	return retval;
}


void base_main_loop() {
	UpdateTime();
	muzak->Listen();
	GFXBeginScene();
	if (createdbase) {

          //		static int i=0;
          //		if (i++%4==3) {
                  createdbase=false;
                  //		}
		AUDStopAllSounds();
	}        
	if (!RefreshGUI()) {
		restore_main_loop();
	}else {
		GFXEndScene();
	}
}



void BaseInterface::Room::Click (BaseInterface* base,float x, float y, int button, int state) {
	if (button==WS_LEFT_BUTTON) {
		int linknum=MouseOver (base,x,y);
		if (linknum>=0) {
			Link * link=links[linknum];
			if (link) {
				link->Click(base,x,y,button,state);
			}
		}
	} else {
#ifdef BASE_MAKER
		if (state==WS_MOUSE_UP) {
			char input [201];
			char *str;
			if (button==WS_RIGHT_BUTTON)
				str="Please create a file named stdin.txt and type\nin the sprite file that you wish to use.";
			else if (button==WS_MIDDLE_BUTTON)
				str="Please create a file named stdin.txt and type\nin the type of room followed by arguments for the room followed by text in quotations:\n1 ROOM# \"TEXT\"\n2 \"TEXT\"\n3 vector<MODES>.size vector<MODES> \"TEXT\"";
			else
				return;
#ifdef _WIN32
			int ret=MessageBox(NULL,str,"Input",MB_OKCANCEL);
#else
			printf("\n%s\n",str);
			int ret=1;
#endif
			int index;
			int rmtyp;
			if (ret==1) {
				if (button==WS_RIGHT_BUTTON) {
#ifdef _WIN32
					FILE *fp=VSFileSystem::vs_open("stdin.txt","rt");
#else
					FILE *fp=stdin;
#endif
					VSFileSystem::vs_fscanf(fp,"%200s",input);
#ifdef _WIN32
					VSFileSystem::vs_close(fp);
#endif
				} else if (button==WS_MIDDLE_BUTTON&&makingstate==0) {
					int i;
#ifdef _WIN32
					FILE *fp=VSFileSystem::vs_open("stdin.txt","rt");
#else
					FILE *fp=stdin;
#endif
	 				VSFileSystem::vs_fscanf(fp,"%d",&rmtyp);
					switch(rmtyp) {
					case 1:
						links.push_back(new Goto("linkind","link"));
						VSFileSystem::vs_fscanf(fp,"%d",&((Goto*)links.back())->index);
						break;
					case 2:
						links.push_back(new Launch("launchind","launch"));
						break;
					case 3:
						links.push_back(new Comp("compind","comp"));
						VSFileSystem::vs_fscanf(fp,"%d",&index);
						for (i=0;i<index&&(!VSFileSystem::vs_feof(fp));i++) {
							VSFileSystem::vs_fscanf(fp,"%d",&ret);
							((Comp*)links.back())->modes.push_back((BaseComputer::DisplayMode)ret);
						}
						break;
					default:
#ifdef _WIN32
						VSFileSystem::vs_close(fp);
						MessageBox(NULL,"warning: invalid basemaker option","Error",MB_OK);
#endif
						printf("warning: invalid basemaker option: %d",rmtyp);
						return;
					}
					VSFileSystem::vs_fscanf(fp,"%200s",input);
					input[200]=input[199]='\0';
					links.back()->text=string(input);
#ifdef _WIN32
					VSFileSystem::vs_close(fp);
#endif
				}
				if (button==WS_RIGHT_BUTTON) {
					input[200]=input[199]='\0';
					objs.push_back(new BaseVSSprite(input,"tex"));
					((BaseVSSprite*)objs.back())->texfile=string(input);
					((BaseVSSprite*)objs.back())->spr.SetPosition(x,y);
				} else if (button==WS_MIDDLE_BUTTON&&makingstate==0) {
					links.back()->x=x;
					links.back()->y=y;
					links.back()->wid=0;
					links.back()->hei=0;
					makingstate=1;
				} else if (button==WS_MIDDLE_BUTTON&&makingstate==1) {
					links.back()->wid=x-links.back()->x;
					if (links.back()->wid<0)
						links.back()->wid=-links.back()->wid;
					links.back()->hei=y-links.back()->y;
					if (links.back()->hei<0)
						links.back()->hei=-links.back()->hei;
					makingstate=0;
				}
			}
		}
#else
		if (state==WS_MOUSE_UP&&links.size()) {
			int count=0;
			while (count++<links.size()) { 
				Link *curlink=links[base->curlinkindex++%links.size()];
				if (curlink) {
					int x=(((curlink->x+(curlink->wid/2))+1)/2)*g_game.x_resolution;
					int y=-(((curlink->y+(curlink->hei/2))-1)/2)*g_game.y_resolution;
                                        biModifyMouseSensitivity(x,y,true);
					winsys_warp_pointer(x,y);
					PassiveMouseOverWin(x,y);
					break;
				}
			}
		}
#endif
	}
}

void BaseInterface::MouseOver (int xbeforecalc, int ybeforecalc) {
	float x, y;
	CalculateRealXAndY(xbeforecalc,ybeforecalc,&x,&y);
	int i=rooms[curroom]->MouseOver(this,x,y);
	Room::Link *link=0;
	if (i<0) {
		link=0;
	} else {
		link=rooms[curroom]->links[i];
	}
	if (link) {
		curtext.SetText(link->text);
		curtext.col=GFXColor(1,.666667,0,1);
		drawlinkcursor=true;
	} else {
		curtext.SetText(rooms[curroom]->deftext);
		curtext.col=GFXColor(0,1,0,1);
		drawlinkcursor=false;
	}
        static bool  draw_always      = XMLSupport::parse_bool(vs_config->getVariable("graphics","base_locationmarker_drawalways","false"));
        static float defined_distance = XMLSupport::parse_float(vs_config->getVariable("graphics","base_locationmarker_distance","0.5"));
		defined_distance = abs(defined_distance);
        if (!draw_always) {
          float cx, cy, wid, hei;
          float dist_cur2link;
          for (i=0; i<rooms[curroom]->links.size(); i++) {
            cx = (rooms[curroom]->links[i]->x + (rooms[curroom]->links[i]->wid / 2));   //get the center of the location
            cy = (rooms[curroom]->links[i]->y + (rooms[curroom]->links[i]->hei / 2));   //get the center of the location
            dist_cur2link = sqrt(pow((cx-x),2) + pow((cy-y),2));
            if ( dist_cur2link < defined_distance ) {
              rooms[curroom]->links[i]->alpha = (1 - (dist_cur2link / defined_distance));
            }
            else {
              rooms[curroom]->links[i]->alpha = 1;
            } //if
          } // for i
        } // if !draw_always
}


void BaseInterface::Click (int xint, int yint, int button, int state) {
	float x,y;
	CalculateRealXAndY(xint,yint,&x,&y);
	rooms[curroom]->Click(this,x,y,button,state);
}

void BaseInterface::ClickWin (int button, int state, int x, int y) {
        ModifyMouseSensitivity(x,y);
	if (CurrentBase) {
		if (CurrentBase->CallComp) {
#ifdef NEW_GUI
			EventManager ::
#else
			UpgradingInfo::
#endif
			               ProcessMouseClick(button,state,x,y);
		} else {
			CurrentBase->Click(x,y,button,state);
		}
	}else {
		NavigationSystem::mouseClick(button,state,x,y);	  
	}
}


void BaseInterface::PassiveMouseOverWin (int x, int y) {
        ModifyMouseSensitivity(x,y);
	SetSoftwareMousePosition(x,y);
	if (CurrentBase) {
		if (CurrentBase->CallComp) {
#ifdef NEW_GUI
			EventManager ::
#else
			UpgradingInfo::
#endif
			               ProcessMousePassive(x,y);
	 	} else {
			CurrentBase->MouseOver(x,y);
		}
	}else {
		NavigationSystem::mouseMotion(x,y);
	}
}

void BaseInterface::ActiveMouseOverWin (int x, int y) {
        ModifyMouseSensitivity(x,y);
	SetSoftwareMousePosition(x,y);
	if (CurrentBase) {
		if (CurrentBase->CallComp) {
#ifdef NEW_GUI
			EventManager ::
#else
			UpgradingInfo::
#endif
			               ProcessMouseActive(x,y);
		} else {
			CurrentBase->MouseOver(x,y);
		}
	}else {
		NavigationSystem::mouseDrag(x,y);
	}
}

void BaseInterface::GotoLink (int linknum) {
	othtext.SetText("");
	if (rooms.size()>linknum&&linknum>=0) {
		curlinkindex=0;
		curroom=linknum;
		curtext.SetText(rooms[curroom]->deftext);
		drawlinkcursor=false;
	} else {
#ifndef BASE_MAKER
		VSFileSystem::vs_fprintf(stderr,"\nWARNING: base room #%d tried to go to an invalid index: #%d",curroom,linknum);
		assert(0);
#else
		while(rooms.size()<=linknum) {
			rooms.push_back(new Room());
			char roomnum [50];
			sprintf(roomnum,"Room #%d",linknum);
			rooms.back()->deftext=roomnum;
		}
		GotoLink(linknum);
#endif
	}
}

BaseInterface::~BaseInterface () {
#ifdef BASE_MAKER
	FILE *fp=VSFileSystem::vs_open("bases/NEW_BASE"BASE_EXTENSION,"wt");
	if (fp) {
		EndXML(fp);
		VSFileSystem::vs_close(fp);
	}
#endif
	CurrentBase=0;
	restore_main_loop();
	for (int i=0;i<rooms.size();i++) {
		delete rooms[i];
	}
}
void base_main_loop();
int shiftup(int);
static void base_keyboard_cb( unsigned int  ch,unsigned int mod, bool release, int x, int y ) {
	if (!release)
		base_keyboard_queue.push_back (((WSK_MOD_LSHIFT==(mod&WSK_MOD_LSHIFT))||(WSK_MOD_RSHIFT==(mod&WSK_MOD_RSHIFT)))?shiftup(ch):ch);
}
void BaseInterface::InitCallbacks () {
	winsys_set_keyboard_func(base_keyboard_cb);	
	winsys_set_mouse_func(ClickWin);
	winsys_set_motion_func(ActiveMouseOverWin);
	winsys_set_passive_motion_func(PassiveMouseOverWin);
	CurrentBase=this;
//	UpgradeCompInterface(caller,baseun);
	CallComp=false;
	static bool simulate_while_at_base=XMLSupport::parse_bool(vs_config->getVariable("physics","simulate_while_docked","false"));
	if (!(simulate_while_at_base||_Universe->numPlayers()>1)) {
		GFXLoop(base_main_loop);
	}
}

BaseInterface::Room::Talk::Talk (std::string ind,std::string pythonfile)
		: BaseInterface::Room::Link(ind,pythonfile) {
	index=-1;
#ifndef BASE_MAKER
	gameMessage last;
	int i=0;
	vector <std::string> who;
	string newmsg;
	string newsound;
	who.push_back ("bar");
	while (( mission->msgcenter->last(i++,last,who))) {
		newmsg=last.message;
		newsound="";
		string::size_type first=newmsg.find_first_of("[");
		{
			string::size_type last=newmsg.find_first_of("]");
			if (first!=string::npos&&(first+1)<newmsg.size()) {
				newsound=newmsg.substr(first+1,last-first-1);
				newmsg=newmsg.substr(0,first);
			}
		}
		this->say.push_back(newmsg);
		this->soundfiles.push_back(newsound);
	}
#endif
}
BaseInterface::Room::Python::Python (std::string ind,std::string pythonfile)
		: BaseInterface::Room::Link(ind,pythonfile) {
}
double compute_light_dot (Unit * base,Unit *un) {
  StarSystem * ss =base->getStarSystem ();
  double ret=-1;
  Unit * st;
  Unit * base_owner=NULL;
  if (ss) {
    _Universe->pushActiveStarSystem (ss);
    un_iter ui = ss->getUnitList().createIterator();
    for (;(st = *ui);++ui) {
      if (st->isPlanet()) {
	if (((Planet *)st)->hasLights()) {
	  QVector v1 = (un->Position()-base->Position()).Normalize();
	  QVector v2 = (st->Position()-base->Position()).Normalize();
	  double dot = v1.Dot(v2);
	  if (dot>ret) {
	    VSFileSystem::vs_fprintf (stderr,"dot %lf",dot);
	    ret=dot;
	  }
	} else {
	  un_iter ui = ((Planet *)st)->satellites.createIterator();
	  Unit * ownz=NULL;
	  for (;(ownz=*ui);++ui) {
	    if (ownz==base) {
	      base_owner = st;
	    }
	  }
	}
      }
    }
    _Universe->popActiveStarSystem();
  }else return 1;
  if (base_owner==NULL||base->isUnit()==PLANETPTR) {
    return ret;
  }else {
    return compute_light_dot(base_owner,un);
  }
}

const char * compute_time_of_day (Unit * base,Unit *un) {
  float rez= compute_light_dot (base,un);
  if (rez>.2) 
    return "day";
  if (rez <-.1)
    return "night";
  return "sunset";
}

extern void ExecuteDirector();
BaseInterface::BaseInterface (const char *basefile, Unit *base, Unit*un)
		: curtext(GFXColor(0,1,0,1),GFXColor(0,0,0,1)) , othtext(GFXColor(1,1,.5,1),GFXColor(0,0,0,1)) {
	CurrentBase=this;
	CallComp=false;
        createdbase=true;
	caller=un;
        curroom=0;
	curlinkindex=0;
	this->baseun=base;
	float x,y;
	curtext.GetCharSize(x,y);
	curtext.SetCharSize(x*2,y*2);
	//	curtext.SetSize(2-(x*4 ),-2);
	curtext.SetSize(1-.01,-2);
	othtext.GetCharSize(x,y);
	othtext.SetCharSize(x*2,y*2);
	//	othtext.SetSize(2-(x*4),-.75);
	othtext.SetSize(1-.01,-.75);

        std::string fac=FactionUtil::GetFaction(base->faction);
        if (fac=="neutral")
          fac  = UniverseUtil::GetGalaxyFaction(UnitUtil::getUnitSystemFile(base));
	Load(basefile, compute_time_of_day(base,un),fac.c_str());
	vector <string> vec;
	vec.push_back(base->name);
	if (un) {
	    int cpt=UnitUtil::isPlayerStarship(un);
		if (cpt>=0) {
			saveStringList(UnitUtil::isPlayerStarship(un),mission_key,vec);
		}
	}
	if (!rooms.size()) {
		VSFileSystem::vs_fprintf(stderr,"ERROR: there are no rooms in basefile \"%s%s%s\" ...\n",basefile,compute_time_of_day(base,un),BASE_EXTENSION);
		rooms.push_back(new Room ());
		rooms.back()->deftext="ERROR: No rooms specified...";
#ifndef BASE_MAKER
		rooms.back()->objs.push_back(new Room::BaseShip (-1,0,0,0,0,-1,0,1,0,QVector(0,0,2),"default room"));
		BaseUtil::Launch(0,"default room",-1,-1,1,2,"ERROR: No rooms specified... - Launch");
		BaseUtil::Comp(0,"default room",0,-1,1,2,"ERROR: No rooms specified... - Computer",
				"Cargo Upgrade Info ShipDealer News Missions");
#endif
	}
	GotoLink(0);
        {
          for (unsigned int i=0;i<16;++i) {
            ExecuteDirector();
          }
        }
}

void BaseInterface::Room::Python::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
		Link::Click(base,x,y,button,state);
//		Do nothing...

	}
}

// Need this for NEW_GUI.  Can't ifdef it out because it needs to link.
void InitCallbacks(void) {
	if(BaseInterface::CurrentBase) {
		BaseInterface::CurrentBase->InitCallbacks();
	}
}
void TerminateCurrentBase(void) {
    BaseInterface::CurrentBase->Terminate();
}
void CurrentBaseUnitSet(Unit * un) {
	if (BaseInterface::CurrentBase) {
		BaseInterface::CurrentBase->caller.SetUnit(un);
	}
}
// end NEW_GUI.

void BaseInterface::Room::Comp::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
		Link::Click(base,x,y,button,state);
		Unit *un=base->caller.GetUnit();
		Unit *baseun=base->baseun.GetUnit();
		if (un&&baseun) {
			base->CallComp=true;
#ifdef NEW_GUI
            BaseComputer* bc = new BaseComputer(un, baseun, modes);
            bc->init();
            bc->run();
#else
			UpgradeCompInterface(un,baseun,modes);
#endif // NEW_GUI
		}
	}
}
void BaseInterface::Terminate() {
  Unit *un=caller.GetUnit();
  int cpt=UnitUtil::isPlayerStarship(un);
  if (un&&cpt>=0) {
    vector <string> vec;
    vec.push_back(string());
    saveStringList(cpt,mission_key,vec);
  }
  BaseInterface::CurrentBase=NULL;
  restore_main_loop();
  delete this;
}
extern void abletodock(int dock);
#include "ai/communication.h"
void BaseInterface::Room::Launch::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
	  Link::Click(base,x,y,button,state);
	  static bool auto_undock = XMLSupport::parse_bool(vs_config->getVariable("physics","AutomaticUnDock","true"));
	  if (auto_undock) {
	  Unit * bas = base->baseun.GetUnit();
	  Unit * playa = base->caller.GetUnit();
	  if (playa &&bas) {
	    playa->UnDock (bas);
	    CommunicationMessage c(bas,playa,NULL,0);
	    c.SetCurrentState (c.fsm->GetUnDockNode(),NULL,0);
		if (playa->getAIState())
			playa->getAIState()->Communicate (c);
	    abletodock(5);
	  }
	  }
	  base->Terminate();
	}
}

void BaseInterface::Room::Goto::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
		Link::Click(base,x,y,button,state);
		base->GotoLink(index);
	}
}

void BaseInterface::Room::Talk::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
		Link::Click(base,x,y,button,state);
		if (index>=0) {
			delete base->rooms[curroom]->objs[index];
			base->rooms[curroom]->objs[index]=NULL;
			index=-1;
			base->othtext.SetText("");
		} else if (say.size()) {
			curroom=base->curroom;
//			index=base->rooms[curroom]->objs.size();
			int sayindex=rand()%say.size();
			base->rooms[curroom]->objs.push_back(new Room::BaseTalk(say[sayindex],"currentmsg",true));
//			((Room::BaseTalk*)(base->rooms[curroom]->objs.back()))->sayindex=(sayindex);
//			((Room::BaseTalk*)(base->rooms[curroom]->objs.back()))->curtime=0;
			if (soundfiles[sayindex].size()>0) {
				int sound = AUDCreateSoundWAV (soundfiles[sayindex],false);
				if (sound==-1) {
					VSFileSystem::vs_fprintf(stderr,"\nCan't find the sound file %s\n",soundfiles[sayindex].c_str());
				} else {
//					AUDAdjustSound (sound,_Universe->AccessCamera ()->GetPosition(),Vector(0,0,0));
					AUDStartPlaying (sound);
					AUDDeleteSound(sound);//won't actually toast it until it stops
				}
			}
		} else {
			VSFileSystem::vs_fprintf(stderr,"\nThere are no things to say...\n");
			assert(0);
		}
	}
}

void BaseInterface::Room::Link::Click (BaseInterface *base,float x, float y, int button, int state) {
	if (state==WS_MOUSE_UP) {
		RunPython(this->pythonfile.c_str());
	}
}

struct BaseColor {
  unsigned char r,g,b,a;
};
static void AnimationDraw() {  
#ifdef RENDER_FROM_TEXTURE
  static StreamTexture T(512,256,NEAREST,NULL);
  BaseColor (* data)[512] = reinterpret_cast<BaseColor(*)[512]>(T.Map());
  bool counter=false;
  srand(time(NULL));
  for (int i=0;i<256;++i) {
    for (int j=0;j<512;++j) {
      data[i][j].r=rand()%255;
      data[i][j].g=rand()%255;
      data[i][j].b=rand()%255;
      data[i][j].a=rand()%255;
    }
  }
  T.UnMap();
  T.MakeActive();
  GFXEnable(TEXTURE0);
  GFXDisable(CULLFACE);
  GFXBegin(GFXQUAD);
  GFXTexCoord2f(0,0);
  GFXVertex3f(-1.0,-1.0,0.0);
  GFXTexCoord2f(1,0);
  GFXVertex3f(1.0,-1.0,0.0);
  GFXTexCoord2f(1,1);
  GFXVertex3f(1.0,1.0,0.0);
  GFXTexCoord2f(0,1);
  GFXVertex3f(-1.0,1.0,0.0);
  GFXEnd();
#endif
}
void BaseInterface::Draw () {
	GFXColor(0,0,0,0);
        SetupViewport();
	StartGUIFrame(GFXTRUE);
        if (GetElapsedTime()<1)
          AnimatedTexture::UpdateAllFrame();
	Room::BaseTalk::hastalked=false;
	rooms[curroom]->Draw(this);
        AnimationDraw();

	float x,y;
        glViewport (0, 0, g_game.x_resolution,g_game.y_resolution);
	curtext.GetCharSize(x,y);
	curtext.SetPos(-.99,-1+(y*1.5));
//	if (!drawlinkcursor)
//		GFXColor4f(0,1,0,1);
//	else
//		GFXColor4f(1,.333333,0,1);
	curtext.Draw();
	othtext.SetPos(-.99,1);
//	GFXColor4f(0,.5,1,1);
	othtext.Draw();
        SetupViewport();
	EndGUIFrame (drawlinkcursor);
        glViewport (0, 0, g_game.x_resolution,g_game.y_resolution);
	Unit *un=caller.GetUnit();
	Unit *base=baseun.GetUnit();
	if (un&&(!base)) {
		VSFileSystem::vs_fprintf(stderr,"Error: Base NULL");
		mission->msgcenter->add("game","all","[Computer] Docking unit destroyed. Emergency launch initiated.");
		for (int i=0;i<un->image->dockedunits.size();i++) {
			if (un->image->dockedunits[i]->uc.GetUnit()==base) {
				un->FreeDockingPort (i);
			}
		}
		Terminate();
	}
}

