/**
 * damageable.cpp
 *
 * Copyright (C) 2020 Roy Falk, Stephen G. Tuggy and other Vega Strike
 * contributors
 *
 * https://github.com/vegastrike/Vega-Strike-Engine-Source
 *
 * This file is part of Vega Strike.
 *
 * Vega Strike is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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


#include "damageable.h"

#include "gfx/vec.h"
#include "vs_math.h"
#include "configuration/game_config.h"
#include "vs_globals.h"
#include "configxml.h"
#include "gfx/vec.h"
#include "lin_time.h"
#include "damageable_factory.h"
#include "damage.h"
#include "unit_generic.h"
#include "ai/communication.h"
#include "universe.h"
#include "ai/order.h"
#include "pilot.h"
#include "ai/comm_ai.h"
#include "gfx/mesh.h"

#include <algorithm>

// TODO: make GameConfig support sub sections
// See https://github.com/vegastrike/Vega-Strike-Engine-Source/issues/358
static float  flickertime = 30.0f; // GameConfig::GetVariable( "graphics", "glowflicker", "time", 30.0f ) );
static float  flickerofftime = 2.0f; // ( "graphics", "glowflicker", "off-time", "2" ) );
static float  minflickercycle  = 2.0f; // ( "graphics", "glowflicker", "min-cycle", "2" ) );
static float  flickeronprob = 0.66f; // ( "graphics", "glowflicker", "num-times-per-second-on", ".66" ) );
static float  hullfornoflicker = 0.04f; // ( "graphics", "glowflicker", "hull-for-total-dark", ".04" ) );




bool Damageable::ShieldUp( const Vector &pnt ) const
{
    const int    shield_min  = 5;

    // TODO: think about this. I have no idea why a nebula needs shields
//    static float nebshields = XMLSupport::parse_float( vs_config->getVariable( "physics", "nebula_shield_recharge", ".5" ) );
//    if (nebula != NULL || nebshields > 0)
//        return false;

    CoreVector attack_vector(pnt.i, pnt.j, pnt.k);

    DamageableLayer shield = const_cast<Damageable*>(this)->GetShieldLayer();
    int facet_index = shield.GetFacetIndex(attack_vector);
    return shield.facets[facet_index].health > shield_min;
}

float Damageable::DealDamageToHull( const Vector &pnt, float damage )
{
    Damage dmg(0, damage); // Bypass shield with phase damage
    CoreVector attack_vector(pnt.i, pnt.j, pnt.k);
    InflictedDamage inflicted_damage(3);
    armor->DealDamage(attack_vector, dmg, inflicted_damage);
    hull->DealDamage(attack_vector, dmg, inflicted_damage);
    int facet_index = armor->GetFacetIndex(attack_vector);

    float denominator = GetArmor(facet_index) + GetHull();
    if(denominator == 0) {
        return 0;
    }

    return damage/denominator;
}

float Damageable::DealDamageToShield( const Vector &pnt, float &damage )
{
    Damage dmg(damage);
    CoreVector attack_vector(pnt.i, pnt.j, pnt.k);
    InflictedDamage inflicted_damage(3);
    shield->DealDamage(attack_vector, dmg, inflicted_damage);
    int facet_index = shield->GetFacetIndex(attack_vector);

    float denominator = GetShield(facet_index) + GetHull();
    if(denominator == 0) {
        return 0;
    }

    return damage/denominator;
}

// TODO: deal with this
extern void ScoreKill( Cockpit *cp, Unit *killer, Unit *killedUnit );

void Damageable::ApplyDamage( const Vector &pnt,
                              const Vector &normal,
                              Damage damage,
                              Unit *affected_unit,
                              const GFXColor &color,
                              void *ownerDoNotDereference)
{
    InflictedDamage inflicted_damage(3);

    const Damageable *const_damagable = static_cast<const Damageable*>(this);
    Unit *unit = static_cast<Unit*>(this);


    static float hull_percent_for_comm = GameConfig::GetVariable( "AI", "HullPercentForComm", 0.75 );
    static int shield_damage_anger = GameConfig::GetVariable( "AI", "ShieldDamageAnger", 1 );
    static int hull_damage_anger   = GameConfig::GetVariable( "AI", "HullDamageAnger", 10 );
    static bool assist_ally_in_need =
        GameConfig::GetVariable( "AI", "assist_friend_in_need", true );
    static float nebula_shields = GameConfig::GetVariable( "physics", "nebula_shield_recharge", 0.5 );
    //We also do the following lock on client side in order not to display shield hits
    static bool no_dock_damage = GameConfig::GetVariable( "physics", "no_damage_to_docked_ships", true );
    static bool apply_difficulty_enemy_damage =
        GameConfig::GetVariable( "physics", "difficulty_based_enemy_damage", true );


    // Stop processing if the affected unit isn't this unit
    // How could this happen? Why even have two parameters (this and affected_unit)???
    if (affected_unit != unit) {
        return;
    }

    // Stop processing for destroyed units
    if(Destroyed()) {
        return;
    }

    // Stop processing for docked ships if no_dock_damage is set
    if (no_dock_damage && (unit->DockedOrDocking()&(unit->DOCKED_INSIDE|unit->DOCKED))) {
        return;
    }


    Cockpit     *shooter_cockpit = _Universe->isPlayerStarshipVoid( ownerDoNotDereference );
    bool shooter_is_player = (shooter_cockpit != nullptr);
    bool shot_at_is_player = _Universe->isPlayerStarship( unit );
    Vector       localpnt( InvTransform( unit->cumulative_transformation_matrix, pnt ) );
    Vector       localnorm( unit->ToLocalCoordinates( normal ) );
    CoreVector attack_vector(localpnt.i,localpnt.j,localpnt.k);
    float previous_hull_percent = GetHullPercent();

    /*float        hullpercent = GetHullPercent();
    bool         mykilled    = Destroyed();
    bool         armor_damage = false;*/

    inflicted_damage = DealDamage(attack_vector, damage);

    if(shooter_is_player) {
        // Why is color relevant here?
        if (color.a != 2 && apply_difficulty_enemy_damage) {
            damage.phase_damage *= g_game.difficulty;
            damage.normal_damage *= g_game.difficulty;
        }

        // Anger Management
        float inflicted_armor_damage = inflicted_damage.inflicted_damage_by_layer[1];
        int anger = inflicted_armor_damage ? hull_damage_anger : shield_damage_anger;

        // If we damage the armor, we do this 10 times by default
        for (int i = 0; i < anger; ++i) {
            //now we can dereference it because we checked it against the parent
            CommunicationMessage c( reinterpret_cast< Unit* > (ownerDoNotDereference), unit, nullptr, 0 );
            c.SetCurrentState( c.fsm->GetHitNode(), nullptr, 0 );
            if ( unit->getAIState() ) unit->getAIState()->Communicate( c );
        }

        //the dark danger is real!
        unit->Threaten( reinterpret_cast< Unit* > (ownerDoNotDereference), 10 );
    } else {
        //if only the damage contained which faction it belonged to
        unit->pilot->DoHit( unit, ownerDoNotDereference, FactionUtil::GetNeutralFaction() );

        // Non-player ships choose a target when hit. Presumably the shooter.
        if (unit->aistate) {
            unit->aistate->ChooseTarget();
        }
    }

    if (Destroyed()) {
        unit->ClearMounts();

        if (shooter_is_player) {
            ScoreKill( shooter_cockpit, reinterpret_cast< Unit* > (ownerDoNotDereference), unit );
        } else {
            Unit *tmp;
            if ( ( tmp = findUnitInStarsystem( ownerDoNotDereference ) ) != nullptr ) {
                if ( ( nullptr != ( shooter_cockpit = _Universe->isPlayerStarshipVoid( tmp->owner ) ) )
                     && (shooter_cockpit->GetParent() != nullptr) )
                    ScoreKill( shooter_cockpit, shooter_cockpit->GetParent(), unit );
                else
                    ScoreKill( NULL, tmp, unit );
            }
        }

        // Additional house cleaning
        unit->PrimeOrders();
        unit->maxenergy = unit->energy = 0;
        unit->Split( rand()%3+1 );


        // Effect on factions
        int neutralfac  = FactionUtil::GetNeutralFaction();
        int upgradesfac = FactionUtil::GetUpgradeFaction();

        // Neutral factions (planets, jump points) and upgrades (turrets) aren't people
        // and can't eject themselves or cargo
        if (unit->faction == neutralfac || unit->faction == upgradesfac) {
            return;
        }

        // Eject cargo
        static float cargo_eject_percent =
            GameConfig::GetVariable( "physics", "eject_cargo_percent", 1.0 );
        static unsigned int max_dump_cargo =
            GameConfig::GetVariable( "physics", "max_dumped_cargo", 15 );
        unsigned int dumped_cargo = 0;


        for (unsigned int i = 0; i < unit->numCargo(); ++i) {
            if (rand() < (RAND_MAX * cargo_eject_percent) &&
                    dumped_cargo++ < max_dump_cargo) {
                  unit->EjectCargo( i );
            }
        }


        // Eject Pilot
        // Can't use this as we can't reach negative hull damage
        //static float hull_dam_to_eject    =
        //    GameConfig::GetVariable( "physics", "hull_damage_to_eject", 100.0 );
        static float auto_eject_percent =
                GameConfig::GetVariable( "physics", "autoeject_percent", 0.5 );
        static bool player_autoeject =
                GameConfig::GetVariable( "physics", "player_autoeject", true );

        if(shot_at_is_player && player_autoeject) {
            unit->EjectCargo( (unsigned int) -1 );
        } else if(rand() < (RAND_MAX*auto_eject_percent) && unit->isUnit() == _UnitType::unit &&
                  unit->faction != neutralfac && unit->faction != upgradesfac) {
            unit->EjectCargo( (unsigned int) -1 );
        }

        return;
    }

    // Light shields if hit
    if(inflicted_damage.inflicted_damage_by_layer[2] > 0) {
        unit->LightShields( pnt, normal, GetShieldPercent(), color );
    }

    // Apply damage to meshes
    // TODO: move to drawable as a function
    if(inflicted_damage.inflicted_damage_by_layer[0] > 0 ||
            inflicted_damage.inflicted_damage_by_layer[1] > 0) {

        for (unsigned int i = 0; i < unit->nummesh(); ++i) {
            // TODO: figure out how to adjust looks for armor damage
            float hull_damage_percent = static_cast<float>(const_damagable->GetHullPercent());
            unit->meshdata[i]->AddDamageFX( pnt, unit->shieldtight ? unit->shieldtight*normal : Vector( 0, 0, 0 ),
                                            hull_damage_percent, color );
        }
    }

    // Shake cockpit
    Cockpit *shot_at_cockpit = _Universe->isPlayerStarship( unit );

    // The second condition should always be met, but if not, at least we won't crash
    if(shot_at_is_player && shot_at_cockpit) {
        if(inflicted_damage.inflicted_damage_by_layer[0] >0 ) {
            // Hull is hit - shake hardest
            shot_at_cockpit->Shake( inflicted_damage.total_damage, 2 );

            unit->playHullDamageSound(pnt);
        } else if(inflicted_damage.inflicted_damage_by_layer[1] >0 ) {
            // Armor is hit - shake harder
            shot_at_cockpit->Shake( inflicted_damage.total_damage, 1 );

            unit->playArmorDamageSound(pnt);
        } else {
            // Shield is hit - shake
            shot_at_cockpit->Shake( inflicted_damage.total_damage, 0 );

            unit->playShieldDamageSound(pnt);
        }
    }

    // Only happens if we crossed the threshold in this attack
    if ( previous_hull_percent >= hull_percent_for_comm &&
         GetHullPercent() < hull_percent_for_comm &&
         ( shooter_is_player || shot_at_is_player ) ) {
        Unit *computer_ai = nullptr;
        Unit *player     = nullptr;
        if (shot_at_is_player) {
            computer_ai = findUnitInStarsystem( ownerDoNotDereference );
            player     = unit;
        } else {
            computer_ai = unit;
            player     = shooter_cockpit->GetParent();
        }

        if(computer_ai && player) {
            Order *computer_ai_state = computer_ai->getAIState();
            Order *player_ai_state = player->getAIState();
            bool ai_is_unit = computer_ai->isUnit() == _UnitType::unit;
            bool player_is_unit = player->isUnit() == _UnitType::unit;
            if (computer_ai && player && computer_ai_state && player_ai_state &&
                ai_is_unit && player_is_unit) {
                unsigned char gender;
                vector< Animation* > *anim = computer_ai->pilot->getCommFaces( gender );
                if (shooter_is_player && assist_ally_in_need) {
                    AllUnitsCloseAndEngage( player, computer_ai->faction );
                }
                if (GetHullPercent() > 0 || !shooter_cockpit) {
                    CommunicationMessage c( computer_ai, player, anim, gender );
                    c.SetCurrentState( shooter_cockpit ? c.fsm->GetDamagedNode() : c.fsm->GetDealtDamageNode(), anim, gender );
                    player->getAIState()->Communicate( c );
                }
            }
        }
    }

    // Damage internal systems
    static bool system_damage_on_armor = GameConfig::GetVariable( "physics", "system_damage_on_armor", false );
    bool hull_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;
    bool armor_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;

    DamageRandomSystem(inflicted_damage, shot_at_is_player, pnt);

    // TODO: lib_damage rewrite non-lethal
    // Note: we really want a complete rewrite together with the modules sub-system
    // Non-lethal/Disabling Weapon code here
    /*static float disabling_constant =
            XMLSupport::parse_float( vs_config->getVariable( "physics", "disabling_weapon_constant", "1" ) );
    if (hull > 0)
        pImage->LifeSupportFunctionality += disabling_constant*damage/hull;
    if (pImage->LifeSupportFunctionality < 0) {
        pImage->LifeSupportFunctionalityMax += pImage->LifeSupportFunctionality;
        pImage->LifeSupportFunctionality     = 0;
        if (pImage->LifeSupportFunctionalityMax < 0)
            pImage->LifeSupportFunctionalityMax = 0;
    }*/

    DamageCargo(inflicted_damage);
  }

// TODO: get rid of extern
extern bool DestroySystem( float hull, float maxhull, float numhits );
extern bool DestroyPlayerSystem( float hull, float maxhull, float numhits );
extern float rand01();
extern const Unit * loadUnitByCache( std::string name, int faction );

void Damageable::DamageRandomSystem(InflictedDamage inflicted_damage, bool player, Vector attack_vector) {
    static bool system_damage_on_armor = GameConfig::GetVariable( "physics", "system_damage_on_armor", false );
    static float system_failure = GameConfig::GetVariable( "physics", "indiscriminate_system_destruction", 0.25 );

    Unit *unit = static_cast<Unit*>(this);

    bool hull_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;
    bool armor_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;

    // It's actually easier to read this condition than the equivalent form
    if(!(hull_damage || (system_damage_on_armor && armor_damage))) {
        return;
    }

    bool damage_system;
    if(player) {
        damage_system = DestroyPlayerSystem(*current_hull, *max_hull, 1);
    } else {
        damage_system = DestroySystem(*current_hull, *max_hull, 1);
    }

    if(!damage_system) {
        return;
    }

    unit->DamageRandSys( system_failure * rand01() +
                   (1-system_failure) * ( 1-((*current_hull) > 0 ? hull_damage/(*current_hull) : 1.0f) ),
                   attack_vector, 1.0f, 1.0f );
}

void Damageable::DamageCargo(InflictedDamage inflicted_damage) {
    // TODO: lib_damage
    // The following code needs to be renabled and placed somewhere
    // Non-lethal/Disabling Weapon code here
    // TODO: enable
    /*static float disabling_constant =
        XMLSupport::parse_float( vs_config->getVariable( "physics", "disabling_weapon_constant", "1" ) );
    if (hull > 0)
      pImage->LifeSupportFunctionality += disabling_constant*damage/hull;
    if (pImage->LifeSupportFunctionality < 0) {
        pImage->LifeSupportFunctionalityMax += pImage->LifeSupportFunctionality;
        pImage->LifeSupportFunctionality     = 0;
        if (pImage->LifeSupportFunctionalityMax < 0)
          pImage->LifeSupportFunctionalityMax = 0;
      }*/


    static bool system_damage_on_armor = GameConfig::GetVariable( "physics", "system_damage_on_armor", false );

    bool hull_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;
    bool armor_damage = inflicted_damage.inflicted_damage_by_layer[0] > 0;

    // TODO: Same condition as DamageRandomSystem - move up and merge
    if(!(hull_damage || (system_damage_on_armor && armor_damage))) {
        return;
    }

    Unit *unit = static_cast<Unit*>(this);

    // If nothing to damage, exit
    if (unit->numCargo() == 0) {
        return;
    }

    // Is the hit unit, lucky or not
    if ( DestroySystem( *current_hull, *max_hull, unit->numCargo() ) ) {
        return;
    }

    static std::string restricted_items = vs_config->getVariable( "physics", "indestructable_cargo_items", "" );
    int cargo_to_damage_index = rand() % unit->numCargo();
    Cargo& cargo = unit->GetCargo( cargo_to_damage_index );
    const std::string& cargo_category = cargo.GetCategory();

    bool is_upgrade = cargo_category.find( "upgrades/" ) == 0;
    bool already_damaged = cargo_category.find( "upgrades/Damaged/" ) == 0;
    bool is_multiple = cargo_category.find( "mult_" ) == 0;
    bool is_restricted = restricted_items.find(cargo.GetContent()) == string::npos;

    // The following comment was kept in the hopes someone else knows what it means
    //why not downgrade _add GetCargo(which).content.find("add_")!=0&&
    if(!is_upgrade || already_damaged || is_multiple || is_restricted) {
        return;
    }


    const int prefix_length = strlen( "upgrades/" );
    cargo.category = "upgrades/Damaged/" + cargo_category.substr(prefix_length);

    // TODO: find a better name for whatever this is. Right now it's not not downgrade
    static bool not_actually_downgrade = GameConfig::GetVariable( "physics", "separate_system_flakiness_component", false );
    if (not_actually_downgrade) {
        return;
    }

    const Unit *downgrade = loadUnitByCache( cargo.content, FactionUtil::GetFactionIndex( "upgrades" ) );
    if (!downgrade) {
        return;
    }

    if ( 0 == downgrade->getNumMounts() && downgrade->SubUnits.empty() ) {
        double percentage = 0;
        unit->Downgrade( downgrade, 0, 0, percentage, nullptr );
    }
}

// TODO: get rid of extern
extern void DestroyMount( Mount* );

// TODO: a lot of this should be handled by RAII
void Damageable::Destroy() {
    Unit *unit = static_cast<Unit*>(this);

    DamageableObject::Destroy();

    if(!unit->killed) {
        for (int beamcount = 0; beamcount < unit->getNumMounts(); ++beamcount) {
            DestroyMount( &unit->mounts[beamcount] );
        }


        if ( !unit->Explode( false, simulation_atom_var ) ) {
            unit->Kill();
        }
    }
}


float Damageable::FShieldData() const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    return static_cast<DamageableLayer>(damageable->GetShieldLayer()).GetPercent(FacetName::front);
}

float Damageable::BShieldData() const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    return static_cast<DamageableLayer>(damageable->GetShieldLayer()).GetPercent(FacetName::rear);
}

float Damageable::LShieldData() const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    return static_cast<DamageableLayer>(damageable->GetShieldLayer()).GetPercent(FacetName::left);
}

float Damageable::RShieldData() const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    return static_cast<DamageableLayer>(damageable->GetShieldLayer()).GetPercent(FacetName::right);
}

//short fix
void Damageable::ArmorData( float armor[8] ) const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    DamageableLayer armor_layer = damageable->GetArmorLayer();
    armor[0] = armor_layer.facets[0].health;
    armor[1] = armor_layer.facets[1].health;
    armor[2] = armor_layer.facets[2].health;
    armor[3] = armor_layer.facets[3].health;
    armor[4] = armor_layer.facets[4].health;
    armor[5] = armor_layer.facets[5].health;
    armor[6] = armor_layer.facets[6].health;
    armor[7] = armor_layer.facets[7].health;
}

// TODO: fix typo
void Damageable::leach( float damShield, float damShieldRecharge, float damEnRecharge )
{
  // TODO: restore this lib_damage
}







float Damageable::MaxShieldVal() const
{
    Damageable *damageable = const_cast<Damageable*>(this);
    return static_cast<DamageableLayer>(damageable->GetShieldLayer()).AverageMaxLayerValue();
}




float Damageable::totalShieldEnergyCapacitance( const DamageableLayer &shield )
{
    float total_max_shield_value = static_cast<DamageableLayer>(shield).TotalMaxLayerValue();
    float total_current_shield_value = static_cast<DamageableLayer>(shield).TotalLayerValue();

    static float shieldenergycap =
        XMLSupport::parse_float( vs_config->getVariable( "physics", "shield_energy_capacitance", ".2" ) );
    static bool  use_max_shield_value =
        XMLSupport::parse_bool( vs_config->getVariable( "physics", "use_max_shield_energy_usage", "false" ) );
    return shieldenergycap*(use_max_shield_value ? total_max_shield_value : total_current_shield_value);
}







bool Damageable::flickerDamage()
{
    float damagelevel = GetHullPercent();
    static double counter = getNewTime();


    float diff = getNewTime()-counter;
    if (diff > flickertime) {
        counter = getNewTime();
        diff    = 0;
    }
    float tmpflicker = flickertime*damagelevel;
    if (tmpflicker < minflickercycle)
        tmpflicker = minflickercycle;
    diff = fmod( diff, tmpflicker );
    //we know counter is somewhere between 0 and damage level
    //cast this to an int for fun!
    unsigned int thus = ( (unsigned int) (size_t) this )>>2;
    thus = thus%( (unsigned int) tmpflicker );
    diff = fmod( diff+thus, tmpflicker );
    if (flickerofftime > diff) {
        if (damagelevel > hullfornoflicker)
            return rand() > RAND_MAX * GetElapsedTime()*flickeronprob;
        else
            return true;
    }
    return false;
}
