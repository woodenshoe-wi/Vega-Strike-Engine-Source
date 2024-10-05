/*
 * jump_drive.h
 *
 * Copyright (C) 2001-2023 Daniel Horn, Benjamen Meyer, Roy Falk, Stephen G. Tuggy,
 * and other Vega Strike contributors.
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
 * along with Vega Strike. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef JUMP_DRIVE_H
#define JUMP_DRIVE_H

#include "component.h"
#include "energy_consumer.h"

class EnergyContainer;

class JumpDrive : public Component, public EnergyConsumer {
    int destination;    // 0 is probably some kind of /dev/null
    double delay;

public:
    JumpDrive();
    JumpDrive(EnergyContainer *source);

    int Destination() const;
    bool IsDestinationSet() const;
    void SetDestination(int destination);
    void UnsetDestination();

    double Delay() const;
    void SetDelay(double delay);

    bool Enabled() const;

    // Component Methods
    virtual void Load(std::string upgrade_key, std::string unit_key);      
    
    virtual void SaveToCSV(std::map<std::string, std::string>& unit) const;

    virtual std::string Describe() const; // Describe component in base_computer 

    virtual bool CanDowngrade() const;

    virtual bool Downgrade();

    virtual bool CanUpgrade(const std::string upgrade_name) const;

    virtual bool Upgrade(const std::string upgrade_name);
};

#endif // JUMP_DRIVE_H
