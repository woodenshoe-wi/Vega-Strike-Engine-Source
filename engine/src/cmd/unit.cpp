/*
 * unit.cpp
 *
 * Copyright (C) 2001-2002 Daniel Horn
 * Copyright (C) 2020-2021 Stephen G. Tuggy and other Vega Strike contributors
 *
 * https://github.com/vegastrike/Vega-Strike-Engine-Source
 *
 * This file is part of Vega Strike.
 *
 * Vega Strike is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Vega Strike is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Vega Strike.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "unit.h"
#include "vs_logging.h"
#include "vs_globals.h"
#include "file_main.h"
#include "gfx/halo.h"
#include "gfx/halo_system.h"
#include "gfx/quaternion.h"
#include "gfx/matrix.h"
#include "gfx/technique.h"
#include "gfx/occlusion.h"



#include "gfx/sprite.h"
#include "lin_time.h"

#include "gfx/vsbox.h"
#include "bolt.h"
#include "gfx/lerp.h"
#include "audiolib.h"
#include "gfx/cockpit.h"
#include "config_xml.h"
#include "images.h"
#include "main_loop.h"
#include "script/mission.h"
#include "script/flightgroup.h"
#include "savegame.h"
#include "xml_serializer.h"
#include "python/python_class.h"
#include "cmd/ai/missionscript.h"
#include "gfx/particle.h"
#include "cmd/ai/aggressive.h"
#include "cmd/base.h"
#include "gfx/point_to_cam.h"

#include "base_util.h"
#include "unit_jump.h"
#include "unit_customize.h"
#include "unit_damage.h"
#include "unit_physics.h"
#include "unit_click.h"
#include "options.h"

#include "weapon_info.h"
#include "mount_size.h"

using std::vector;
using std::string;
using std::map;

//if the PQR of the unit may be variable...for radius size computation
//#define VARIABLE_LENGTH_PQR

//#define DESTRUCTDEBUG
#include "beam.h"
#include "python/init.h"
#include "unit_const_cache.h"
extern double interpolation_blend_factor;
extern double saved_interpolation_blend_factor;
extern bool   cam_setup_phase;

/**** MOVED FROM BASE_INTERFACE.CPP ****/
extern string getCargoUnitName( const char *name );

GameUnit::GameUnit( int )
{
    this->Unit::Init();
}

GameUnit::GameUnit( std::vector< Mesh* > &meshes, bool SubU, int fact ) :
    Unit( meshes, SubU, fact )
{}

GameUnit::GameUnit( const char *filename,
                                                           bool SubU,
                                                           int faction,
                                                           std::string unitModifications,
                                                           Flightgroup *flightgrp,
                                                           int fg_subnumber,
                                                           string *netxml )
{
    Unit::Init( filename, SubU, faction, unitModifications, flightgrp, fg_subnumber, netxml );
}

GameUnit::~GameUnit()
{
    for (unsigned int meshcount = 0; meshcount < this->meshdata.size(); meshcount++)
        if (this->meshdata[meshcount]) {
            delete this->meshdata[meshcount];
            this->meshdata[meshcount] = nullptr;
        }
    this->meshdata.clear();
    //delete phalos;
}

/*
unsigned int GameUnit::nummesh() const {
    // return number of meshes but not the shield
    return (this->meshdata.size() - 1 );
}*/


void GameUnit::UpgradeInterface( Unit *baseun )
{
    string basename = ( ::getCargoUnitName( baseun->getFullname().c_str() ) );
    if (baseun->isUnit() != _UnitType::planet)
        basename = baseun->name;
    BaseUtil::LoadBaseInterfaceAtDock( basename, baseun, this );
}

#define PARANOIA .4

inline static float perspectiveFactor( float d )
{
    if (d > 0)
        return g_game.x_resolution*GFXGetZPerspective( d );
    else
        return 1.0f;
}


VSSprite*GameUnit::getHudImage() const
{
    return this->pImage->pHudImage;
}


void GameUnit::addHalo( const char *filename,
                                    const Matrix &trans,
                                    const Vector &size,
                                    const GFXColor &col,
                                    std::string halo_type,
                                    float halo_speed )
{
    halos->AddHalo( filename, trans, size, col, halo_type, halo_speed );
}


void GameUnit::Cloak( bool engage )
{
    Unit::Cloak( engage );         //client side unit
}


bool GameUnit::queryFrustum( double frustum[6][4] ) const
{
    unsigned int    i;
#ifdef VARIABLE_LENGTH_PQR
    Vector TargetPoint( cumulative_transformation_matrix[0],
                        cumulative_transformation_matrix[1],
                        cumulative_transformation_matrix[2] );
    float  SizeScaleFactor = sqrtf( TargetPoint.Dot( TargetPoint ) );
#else
    Vector TargetPoint;
#endif
    for (i = 0; i < nummesh() && this->meshdata[i]; i++) {
        TargetPoint = Transform( this->cumulative_transformation_matrix, this->meshdata[i]->Position() );
        if ( GFXSphereInFrustum( frustum,
                                 TargetPoint,
                                 this->meshdata[i]->rSize()
#ifdef VARIABLE_LENGTH_PQR
                                 *SizeScaleFactor
#endif
                               ) )
            return true;
    }
    const Unit *un;
    for (un_fkiter iter = this->SubUnits.constFastIterator(); (un = *iter); ++iter)
        if ( ( (GameUnit*)un )->queryFrustum( frustum ) )
            return true;
    return false;
}















