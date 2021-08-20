#include "health.h"

#include <algorithm>
#include <iostream>



void Health::AdjustPower(const float& percent) {
    if(!regenerative) {
        // Not applicable for armor and hull
        return;
    }

    if(percent > 1 || percent < 0) {
        // valid values are between 0 and 1
        return;
    }

    adjusted_health = max_health * percent;

    if(adjusted_health < health) {
        health = adjusted_health;
    }
}

void Health::DealDamage( Damage &damage, InflictedDamage& inflicted_damage ) {
    // If this layer is destroyed, it can no longer sustain damage
    if(destroyed) {
        return;
    }

    //std::cout << std::string:: "pre-deal normal_damage " << damage.normal_damage <<
    //          " health " << health << std::endl;

    DealDamageComponent(0, damage.normal_damage, vulnerabilities.normal_damage, inflicted_damage);
    DealDamageComponent(1, damage.phase_damage, vulnerabilities.phase_damage, inflicted_damage);

    // TODO: implement other types of damage
    if(layer == 0 && destroyed) {
        std::cout << " - health  - ship destroyed\n";
    }

}


/**
 * @brief Health::DealDamageComponent - deal a component of damage (normal, phased) and not damage
 * a component.
 * @param health - to subtract from
 * @param damage - to inflict
 * @param vulnerability - adjust for
 */
// TODO: type is ugly hack
void Health::DealDamageComponent( int type, float &damage, float vulnerability, InflictedDamage& inflicted_damage ) {
    // Here we adjust for specialized weapons such as shield bypassing and shield leeching
    // which only damage the shield.
    // We also cap the actual damage at the current health
    const float adjusted_damage = std::min(damage * vulnerability, health);

    // We check if there's any damage left to pass on to the next layer
    damage -= adjusted_damage;

    // Damage the current health
    health -= adjusted_damage;

    // Record damage
    switch (type) {
    case 0:
        inflicted_damage.normal_damage += adjusted_damage;
        break;
    case 1:
        inflicted_damage.phase_damage += adjusted_damage;
    }

    std::cout << "crash on type " << type << " adjusted " << adjusted_damage << "\n";
    inflicted_damage.total_damage += adjusted_damage;
    inflicted_damage.inflicted_damage_by_layer[layer] += adjusted_damage;

    if(health == 0 && !regenerative) {
        destroyed = true;
    }
}

void Health::Disable() {
    if(regenerative && enabled) {
        enabled = false;
    }
}

void Health::Enable() {
    if(regenerative && !enabled) {
        enabled = true;
    }
}

void Health::ReduceLayerMaximum(const float& percent) {
    max_health = std::max(0.0f, max_health - factory_max_health * percent);
    health = std::min(health, max_health);
}

void Health::ReduceRegeneration(const float& percent) {
    regeneration = std::max(0.0f, regeneration - factory_regeneration * percent);
}

void Health::Regenerate() {
    if(!enabled || destroyed || !regenerative) {
        return;
    }

    health = std::min(max_health, health + regeneration);
}
