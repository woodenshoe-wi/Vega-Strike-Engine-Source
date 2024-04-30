/*
 * resource_tests.cpp
 *
 * Copyright (C) 2001-2024 Daniel Horn, Benjaman Meyer, Roy Falk, Stephen G. Tuggy,
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

#include <gtest/gtest.h>

#include "resource.h"


TEST(Resource, Operators) {
    // Constructor, Value
    Resource<double> resource = Resource<double>(10.0, 0.0, 10.0);
    EXPECT_EQ(resource.Value(), 10.0);

    // Set, operator=
    resource.Set(5.0);
    EXPECT_EQ(resource, 5.0);

    // Other operator=
    resource = 2.0;
    EXPECT_EQ(resource, 2.0);

    // Operator +=
    resource += 5.0;
    EXPECT_EQ(resource, 7.0);

    // Test max checks
    resource += 5.0;
    EXPECT_EQ(resource, 10.0);

    // Operator -=
    resource -= 2.0f;
    EXPECT_EQ(resource, 8.0);

    // Test min checks
    resource -= 12.0;
    EXPECT_EQ(resource, 0.0);

    // Operator ++
    resource++;
    EXPECT_EQ(resource, 1.0);

    resource = 10.0;
    resource++;
    EXPECT_EQ(resource, 10.0);

    // Operator --
    resource--;
    EXPECT_EQ(resource, 9.0);

    // --Operator 
    --resource;
    EXPECT_EQ(resource, 8.0);

    resource = 0.0;
    resource--;
    EXPECT_EQ(resource, 0.0);
    --resource;
    EXPECT_EQ(resource, 0.0);
}


TEST(Resource, Methods) {
    Resource<double> resource = Resource<double>(10.0, 0.0, 10.0);

    // Percent
    resource = 0.0;
    EXPECT_EQ(resource.Percent(), 0.0);

    // SetAdjustedMaxValue
    resource = 10.0;
    resource.SetAdjustedMaxValue(7.0);
    EXPECT_EQ(resource, 7.0);

    // ResetMaxValue
    resource.ResetMaxValue();
    EXPECT_EQ(resource, 7.0);
    EXPECT_EQ(resource.AdjustedValue(), 10.0);

    // SetMaxValue
    resource.SetMaxValue(12.0);
    EXPECT_EQ(resource.MaxValue(), 12.0);
    EXPECT_EQ(resource, 12.0);
    EXPECT_EQ(resource.AdjustedValue(), 12.0);

    // Value, MaxValue, MinValue, AdjustedValue
    resource.SetAdjustedMaxValue(8.0);
    resource = 4.0;
    EXPECT_EQ(resource.MaxValue(), 12.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 8.0);
    EXPECT_EQ(resource.Value(), 4.0);

    Resource<int> int_resource = Resource<int>(10, 0);
    int to_int = int_resource;
    EXPECT_EQ(to_int, 10);
}

TEST(Resource, Damage_Repair) {
    Resource<double> resource = Resource<double>(10.0, 0.0, 10.0);

    // Destroy/Destroyed
    EXPECT_FALSE(resource.Destroyed());
    resource.Destroy();
    EXPECT_TRUE(resource.Destroyed());

    // Repair Fully
    resource.RepairFully();
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 10.0);
    EXPECT_EQ(resource.Value(), 10.0);

    // Damage by value
    resource.DamageByValue(2.0);
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 8.0);
    EXPECT_EQ(resource.Value(), 8.0);

    // Damage by percent
    resource.DamageByPercent(0.2);
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 6.0);
    EXPECT_EQ(resource.Value(), 6.0);

    // Random Damage
    resource.RandomDamage();
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_LT(resource.AdjustedValue(), 6.0);
    EXPECT_LT(resource.Value(), 6.0);
    EXPECT_GE(resource.AdjustedValue(), 0.0);
    EXPECT_GE(resource.Value(), 0.0);

    // Repair 
    resource.Zero();
    resource.RepairByValue(2.0);
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 2.0);
    EXPECT_EQ(resource.Value(), 2.0);

    resource.RepairByPercent(0.2);
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 4.0);
    EXPECT_EQ(resource.Value(), 4.0);

    resource.RepairFully();
    EXPECT_EQ(resource.MaxValue(), 10.0);
    EXPECT_EQ(resource.MinValue(), 0.0);
    EXPECT_EQ(resource.AdjustedValue(), 10.0);
    EXPECT_EQ(resource.Value(), 10.0);
}
