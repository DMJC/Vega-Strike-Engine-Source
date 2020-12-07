#include "missile.h"

#include "universe_util.h"
#include "unit_generic.h"
#include "vegastrike.h"
#include "vs_globals.h"
#include "configxml.h"
#include "images.h"
#include "collection.h"
#include "star_system_generic.h"
#include "role_bitmask.h"
#include "ai/order.h"
#include "faction_generic.h"
#include "unit_util.h"
#include "vsfilesystem.h"
#include "star_system.h"
#include "universe.h"
#include "game_config.h"


////////////////////////////////////////////////////////////////
/// MissileEffect
////////////////////////////////////////////////////////////////
MissileEffect::MissileEffect( const QVector &pos, float dam, float pdam, float radius, float radmult, void *owner ): pos( pos )
{
    damage = dam;
    phasedamage = pdam;
    this->radius     = radius;
    radialmultiplier = radmult;
    this->ownerDoNotDereference = owner;
}

void MissileEffect::ApplyDamage( Unit *smaller )
{
    QVector norm = pos-smaller->Position();
    float smaller_rsize = smaller->rSize();
    float distance = norm.Magnitude()-smaller_rsize;            // no better check than the bounding sphere for now
    if ( distance < radius) {                                   // "smaller->isUnit() != _UnitType::missile &&" was removed - why disable antimissiles?
        if ( distance < 0)
            distance = 0.f;                                     //it's inside the bounding sphere, so we'll not reduce the effect
        if (radialmultiplier < .001) radialmultiplier = .001;
        float dist_part=distance/radialmultiplier;              //radialmultiplier is radius of the set damage
        float damage_mul;
        if ( dist_part > 1.f) {                                  // there can be something else, such as different eye- and ear- candy
            damage_mul = 1/(dist_part*dist_part);
        }
        else {
            damage_mul = 2.f - dist_part*dist_part;
        }
        /*
         *  contrived formula to create paraboloid falloff rather than quadratic peaking at 2x damage at origin
         *  k = 2-distance^2/radmul^2
         *
         * if the explosion itself was a weapon, it would have double the base damage, longrange=0.5 (counted at Rm) and more generic form:
         * Kclose = 1-(1-longrange)*(R/Rm)^2
         * Kfar   = longrange/(R/Rm)^2
         * or Kapprox = longrange/(longrange-(R/Rm)^3*(1-longrange)) ; obviously, with more checks preventing /0
         */
        DoApplyDamage(smaller, smaller, distance, damage_mul);
    }
}

const QVector&  MissileEffect::GetCenter() const
{
    return pos;
}


float  MissileEffect::GetRadius() const
{
    return radius;
}


void MissileEffect::DoApplyDamage(Unit *parent, Unit *un, float distance, float damage_fraction) {
    QVector norm = pos-un->Position();
    norm.Normalize();
    float damage_left = 1.f;
    if (un->hasSubUnits()) {
        /*
         * Compute damage aspect ratio of each subunit with their apparent size ( (radius/distance)^2 )
         * and spread damage across affected subunits based on their apparent size vs total spread surface
         */
        double total_area = 0.0f; {
            un_kiter ki = un->viewSubUnits();
            for (const Unit *subun; (subun = *ki); ++ki) {
                if (subun->Killed()) continue;
                double r = subun->rSize();
                double d = (pos - subun->Position()).Magnitude() - r;
                if (d > radius) continue;
                if (d < 0.01) d = 0.01;
                total_area += (r*r) / (d*d);
            }
        }
        if (total_area > 0) {
            BOOST_LOG_TRIVIAL(info) << boost::format("Missile subunit damage of %1$.3f%%") % (total_area * (100.0 / 4.0*M_PI));
        }
        if (total_area < 4.0*M_PI) total_area = 4.0*M_PI;

        un_iter i = un->getSubUnits();
        for (Unit *subun; (subun = *i); ++i) {
            if (subun->Killed()) continue;
            double r = subun->rSize();
            double d = (pos - subun->Position()).Magnitude() - r;
            if (d > radius) continue;
            if (d < 0.01) d = 0.01;
            double a = (r*r) / (d*d*total_area);
            DoApplyDamage(parent, subun, d, a * damage_fraction);
            damage_left -= a;
        }
    }
    if (damage_left > 0) {
        BOOST_LOG_TRIVIAL(info)
                << boost::format("Missile damaging %1%/%2% (dist=%3$.3f r=%4$.3f dmg=%5$.3f)")
                % parent->name.get()
                % ((un == parent) ? "." : un->name.get())
                % distance
                % radius
                % (damage*damage_fraction*damage_left);
        parent->ApplyDamage( pos.Cast(), norm, damage*damage_fraction*damage_left, un, GFXColor( 1,1,1,1 ),
                             ownerDoNotDereference, phasedamage*damage_fraction*damage_left );
    }
}



////////////////////////////////////////////////////////////////
/// Missile
////////////////////////////////////////////////////////////////

Missile::Missile( const char *filename,
             int faction,
             const string &modifications,
             const float damage,
             float phasedamage,
             float time,
             float radialeffect,
             float radmult,
             float detonation_radius ) :
    GameUnit( filename, false, faction, modifications )
  , time( time )
  , damage( damage )
  , phasedamage( phasedamage )
  , radial_effect( radialeffect )
  , radial_multiplier( radmult )
  , detonation_radius( detonation_radius )
  , discharged( false )
  , retarget( -1 )
  , had_target ( false )

{
    static bool missilesparkle = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "missilesparkle", "false" ) );
    if (missilesparkle)
        maxhull *= 4;
}


void Missile::Discharge() {
    if ( (damage != 0 || phasedamage != 0) && !discharged ) {
        Unit *target = Unit::Target();
        BOOST_LOG_TRIVIAL(info) << boost::format("Missile discharged (target %1%)")
                                % ((target != NULL) ? target->name.get() : "NULL");
        _Universe->activeStarSystem()->AddMissileToQueue(
            new MissileEffect( Position(), damage, phasedamage,
            radial_effect, radial_multiplier, owner ) );
    discharged = true;
  }
}


float Missile::ExplosionRadius()
{
    static float missile_multiplier =
        XMLSupport::parse_float( vs_config->getVariable( "graphics", "missile_explosion_radius_mult", "1" ) );

    return radial_effect*(missile_multiplier);
}





void Missile::Kill( bool erase ) {
    Discharge();
    GameUnit::Kill( erase );
}


void Missile::UpdatePhysics2( const Transformation &trans,
                              const Transformation &old_physical_state,
                              const Vector &accel,
                              float difficulty,
                              const Matrix &transmat,
                              const Vector &CumulativeVelocity,
                              bool ResolveLast,
                              UnitCollection *uc )
{
    // First we move the missile by calling the super
    GameUnit::UpdatePhysics2( trans, old_physical_state, accel, difficulty, transmat, CumulativeVelocity, ResolveLast, uc );


    // Get the target
    Unit *target = Target();

    // We have a valid target - we mark this as it affects fuel and TTL
    if(target != nullptr)
    {
        had_target = true;
    }

    // Try and break lock with ECM
    target = breakECMLock(target);

    // We have a valid target
    if(target != nullptr)
    {
        // But the target is dead already
        if (target->hull < 0) {
            target = nullptr;
        } else {
            // What does this do?
            static unsigned int pointdef = ROLES::getRole( "POINTDEF" );
            target->graphicOptions.missilelock = true;
            un_iter i = target->getSubUnits();

            Unit   *su;
            for (; (su = *i) != nullptr; ++i)
            {
                if (su->attackPreference() == pointdef)
                {
                    if (su->Target() == nullptr)
                    {
                        float speed, range, mrange;
                        su->getAverageGunSpeed( speed, range, mrange );
                        if ( ( Position()-su->Position() ).MagnitudeSquared() < range*range )
                        {
                            su->Target( this );
                            su->TargetTurret( this );
                        }
                    }
                }
            }
        }
    }

    // Go wild
    // I'm pretty sure this code does nothing so I'm commenting it out
    /*if (retarget == -1)
    {
        if (target)
            retarget = 1;
        else
            retarget = 0;
    }

    if (retarget && target == nullptr)
    {
        Target( nullptr );
    }*/

    if (proximityFuse(target))
    {
        return;
    }

    if (useFuel(target, had_target))
    {
        return;
    }
}

Unit* Missile::breakECMLock(Unit* target)
{
    if(target == nullptr)
    {
        return nullptr;
    }

    // There are two separate checks for ECM breaking lock here
    // TODO: pick one
    float r = rand();
    float rand_max = static_cast<float>(RAND_MAX);
    float ecm_value = static_cast<float>(UnitUtil::getECM(target)) * SIMULATION_ATOM/32768; // should it be simulation_atom_var?

    if (r/rand_max < ecm_value)
    {
        return nullptr;
    }

    // Second check
    static size_t max_ecm = static_cast<size_t>(GameConfig::GetVariable( "physics", "max_ecm", 4));
    size_t missile_hash = reinterpret_cast<size_t>(this)/16383;

    if( static_cast<int>(missile_hash%max_ecm) < UnitUtil::getECM(target))
    {
        return nullptr;
    }

    return target;
}

bool Missile::proximityFuse(Unit* target)
{
    // Don't explode if have no lock or already exploded/exploding
    if(target == nullptr || discharged)
    {
        return false;
    }


    QVector endpos = Position();
    QVector startpos = endpos-( simulation_atom_var*GetVelocity() );

    // checker - checks proximity fuse?
    float checker = target->querySphere( startpos, endpos, rSize() );
    if ( checker && detonation_radius >= 0 ) {
        // Set position to the collision point
        SetPosition(startpos + (endpos - startpos) * checker);

        //spiritplumber assumes that the missile is hitting a much larger object than itself
        // It seems spiritplumber is a former dev of the project.
        static float percent_missile_match_target_velocity =
                GameConfig::GetVariable( "physics", "percent_missile_match_target_velocity", 1.0);
        Velocity += percent_missile_match_target_velocity*(target->Velocity - Velocity);

        Discharge();
        time = -1;
        return true; // We've exploded
    }

    return false;
}

bool Missile::useFuel(Unit* target, bool had_target)
{
    static float max_lost_target_live_time =
        GameConfig::GetVariable( "physics", "max_lost_target_live_time", 30);

    // An previous dev marked this as BROKEN
    // If we had a target but it's now gone, limit the missile's fuel
    // If we didn't have a target (dumbfire?), keep original fuel
    if ( had_target && target == nullptr )
    {
        time = std::min(time, max_lost_target_live_time);
    }

    // Reduce missile TTL/Fuel by tick
    time -= simulation_atom_var;

    // Have we run out of fuel? If so, terminate the missile without exploding
    // Another way of killing the missile, this time if it runs out of fuel
    if (time < 0)
    {
        // TODO: This really should be kill()
        DealDamageToHull( Vector( .1f, .1f, .1f ), hull+1 );
        return true;
    }

    return false;
}
