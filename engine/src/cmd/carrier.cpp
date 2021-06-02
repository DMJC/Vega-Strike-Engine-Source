#include "carrier.h"

#include "unit_generic.h"
#include "unit.h"
#include "universe.h"
#include "universe_util.h"

#include "ai/aggressive.h"
#include "csv.h"
#include "missile.h"
#include "vs_random.h"

// TODO: find out where this is and maybe refactor
extern int SelectDockPort( Unit*, Unit *parent );
extern void SwitchUnits( Unit*, Unit* );
extern void abletodock( int dock );
extern void UpdateMasterPartList( Unit *ret );

// Replace with std:sto* here and at unit_csv.cpp
static double stof( const string &inp, double def = 0 )
{
    if (inp.length() != 0)
        return XMLSupport::parse_float( inp );
    return def;
}

static int stoi( const string &inp, int def = 0 )
{
    if (inp.length() != 0)
        return XMLSupport::parse_int( inp );
    return def;
}



// TODO: probably replace with a lambda expression
class CatCompare
{
public:
    bool operator()( const Cargo &a, const Cargo &b )
    {
        std::string::const_iterator aiter = a.GetCategory().begin();
        std::string::const_iterator aend  = a.GetCategory().end();
        std::string::const_iterator biter = b.GetCategory().begin();
        std::string::const_iterator bend  = b.GetCategory().end();
        for (; aiter != aend && biter != bend; ++aiter, ++biter) {
            char achar = *aiter;
            char bchar = *biter;
            if (achar < bchar)
                return true;
            if (achar > bchar)
                return false;
        }
        return false;
    }
};


// TODO: move these two functions to vector and make into single constructor
inline float uniformrand( float min, float max )
{
    return ( (float) ( rand() )/RAND_MAX )*(max-min)+min;
}

inline QVector randVector( float min, float max )
{
    return QVector( uniformrand( min, max ),
                   uniformrand( min, max ),
                   uniformrand( min, max ) );
}

std::string CargoToString( const Cargo &cargo )
{
    string missioncargo;
    if (cargo.mission)
        missioncargo = string( "\" missioncargo=\"" )+XMLSupport::tostring( cargo.mission );
    return string( "\t\t\t<Cargo mass=\"" )+XMLSupport::tostring( (float) cargo.mass )+string( "\" price=\"" )
           +XMLSupport::tostring( (float) cargo.price )+string( "\" volume=\"" )+XMLSupport::tostring( (float) cargo.volume )
           +string(
               "\" quantity=\"" )+XMLSupport::tostring( (int) cargo.quantity )+string( "\" file=\"" )+cargo.GetContent()
           +missioncargo
           +string( "\"/>\n" );
}


Carrier::Carrier()
{

}

void Carrier::SortCargo()
{
    // TODO: better cast
    Unit *un = (Unit*)this;
    std::sort( un->pImage->cargo.begin(), un->pImage->cargo.end() );
    for (unsigned int i = 0; i+1 < un->pImage->cargo.size(); ++i)
        if (un->pImage->cargo[i].content == un->pImage->cargo[i+1].content) {
            float tmpmass   = un->pImage->cargo[i].quantity*un->pImage->cargo[i].mass
                              +un->pImage->cargo[i+1].quantity*un->pImage->cargo[i+1].mass;
            float tmpvolume = un->pImage->cargo[i].quantity*un->pImage->cargo[i].volume
                              +un->pImage->cargo[i+1].quantity*un->pImage->cargo[i+1].volume;
            un->pImage->cargo[i].quantity += un->pImage->cargo[i+1].quantity;
            if (un->pImage->cargo[i].quantity) {
                tmpmass   /= un->pImage->cargo[i].quantity;
                tmpvolume /= un->pImage->cargo[i].quantity;
            }
            un->pImage->cargo[i].volume  = tmpvolume;
            un->pImage->cargo[i].mission = (un->pImage->cargo[i].mission || un->pImage->cargo[i+1].mission);
            un->pImage->cargo[i].mass    = tmpmass;
            //group up similar ones
            un->pImage->cargo.erase( un->pImage->cargo.begin()+(i+1) );
            i--;
        }
}

std::string Carrier::cargoSerializer( const XMLType &input, void *mythis )
{
    Unit *un = (Unit*) mythis;
    if (un->pImage->cargo.size() == 0)
        return string( "0" );
    un->SortCargo();
    string retval( "" );
    if ( !( un->pImage->cargo.empty() ) ) {
        retval = un->pImage->cargo[0].GetCategory()+string( "\">\n" )+CargoToString( un->pImage->cargo[0] );
        for (unsigned int kk = 1; kk < un->pImage->cargo.size(); ++kk) {
            if (un->pImage->cargo[kk].category != un->pImage->cargo[kk-1].category)
                retval += string( "\t\t</Category>\n\t\t<Category file=\"" )+un->pImage->cargo[kk].GetCategory()+string(
                    "\">\n" );
            retval += CargoToString( un->pImage->cargo[kk] );
        }
        retval += string( "\t\t</Category>\n\t\t<Category file=\"nothing" );
    } else {
        retval = string( "nothing" );
    }
    return retval;
}

//index in here is unsigned, UINT_MAX and UINT_MAX-1 seem to be
//special states.  This means the total amount of cargo any ship can have
//is UINT_MAX -3   which is 65532 for 32bit machines.
void Carrier::EjectCargo( unsigned int index )
{
    Unit *unit = static_cast<Unit*>(this);
    Cargo *tmp = NULL;
    Cargo  ejectedPilot;
    Cargo  dockedPilot;
    string name;
    bool   isplayer = false;
    //if (index==((unsigned int)-1)) { is ejecting normally
    //if (index==((unsigned int)-2)) { is ejecting for eject-dock

    Cockpit *cp = NULL;
    if ( index == (UINT_MAX-1) ) {
//        _Universe->CurrentCockpit();
        //this calls the unit's existence, by the way.
        name = "return_to_cockpit";
        if ( NULL != ( cp = _Universe->isPlayerStarship( unit ) ) ) {
            isplayer = true;
        }
        //we will have to check for this on undock to return to the parent unit!
        dockedPilot.content = "return_to_cockpit";
        dockedPilot.mass    = .1;
        dockedPilot.volume  = 1;
        tmp = &dockedPilot;
    }
    if (index == UINT_MAX) {
        int pilotnum = _Universe->CurrentCockpit();
        name = "Pilot";
        if ( NULL != ( cp = _Universe->isPlayerStarship( unit ) ) ) {
            string playernum = string( "player" )+( (pilotnum == 0) ? string( "" ) : XMLSupport::tostring( pilotnum ) );
            isplayer = true;
        }
        ejectedPilot.content = "eject";
        ejectedPilot.mass    = .1;
        ejectedPilot.volume  = 1;
        tmp = &ejectedPilot;
    }
    if ( index < numCargo() )
        tmp = &GetCargo( index );
    static float cargotime = XMLSupport::parse_float( vs_config->getVariable( "physics", "cargo_live_time", "600" ) );
    if (tmp) {
        string    tmpcontent = tmp->content;
        if (tmp->mission)
            tmpcontent = "Mission_Cargo";
        const int ulen = strlen( "upgrades" );
        //prevents a number of bad things, incl. impossible speeds and people getting rich on broken stuff
        if ( (!tmp->mission) && memcmp( tmp->GetCategory().c_str(), "upgrades", ulen ) == 0 )
            tmpcontent = "Space_Salvage";
        //this happens if it's a ship
        if (tmp->quantity > 0) {
            const int sslen = strlen( "starships" );
            Unit     *cargo = NULL;
            if (tmp->GetCategory().length() >= (unsigned int) sslen) {
                if ( (!tmp->mission) && memcmp( tmp->GetCategory().c_str(), "starships", sslen ) == 0 ) {
                    string ans = tmpcontent;
                    string::size_type blank = ans.find( ".blank" );
                    if (blank != string::npos)
                        ans = ans.substr( 0, blank );
                    Flightgroup *fg = unit->getFlightgroup();
                    int fgsnumber   = 0;
                    if (fg != NULL) {
                        fgsnumber = fg->nr_ships;
                        ++(fg->nr_ships);
                        ++(fg->nr_ships_left);
                    }
                    cargo = new GameUnit( ans.c_str(), false, unit->faction, "", fg, fgsnumber, NULL );
                    cargo->PrimeOrders();
                    cargo->SetAI( new Orders::AggressiveAI( "default.agg.xml" ) );
                    cargo->SetTurretAI();
                    //he's alive!!!!!
                }
            }
            float arot = 0;
            static float grot =
                XMLSupport::parse_float( vs_config->getVariable( "graphics", "generic_cargo_rotation_speed",
                                                                 "1" ) )*3.1415926536/180;
            if (!cargo) {
                static float crot =
                    XMLSupport::parse_float( vs_config->getVariable( "graphics", "cargo_rotation_speed",
                                                                     "60" ) )*3.1415926536/180;
                static float erot =
                    XMLSupport::parse_float( vs_config->getVariable( "graphics", "eject_rotation_speed",
                                                                     "0" ) )*3.1415926536/180;
                if (tmpcontent == "eject") {
                    if (isplayer) {
                        Flightgroup *fg = unit->getFlightgroup();
                        int fgsnumber   = 0;
                        if (fg != NULL) {
                            fgsnumber = fg->nr_ships;
                            ++(fg->nr_ships);
                            ++(fg->nr_ships_left);
                        }
                        cargo = new GameUnit( "eject", false, unit->faction, "", fg, fgsnumber, NULL);
                    } else {
                        int fac = FactionUtil::GetUpgradeFaction();
                        cargo = new GameUnit( "eject", false, fac, "", NULL, 0, NULL );
                    }
                    if (unit->owner)
                        cargo->owner = unit->owner;
                    else
                        cargo->owner = unit;
                    arot = erot;
                    static bool eject_attacks = XMLSupport::parse_bool( vs_config->getVariable( "AI", "eject_attacks", "false" ) );
                    if (eject_attacks) {
                        cargo->PrimeOrders();
                        //generally fraidycat AI
                        cargo->SetAI( new Orders::AggressiveAI( "default.agg.xml" ) );
                    }

                    //Meat. Docking should happen here
                } else if (tmpcontent == "return_to_cockpit") {
                    if (isplayer) {
                        Flightgroup *fg = unit->getFlightgroup();
                        int fgsnumber   = 0;
                        if (fg != NULL) {
                            fgsnumber = fg->nr_ships;
                            ++(fg->nr_ships);
                            ++(fg->nr_ships_left);
                        }
                        cargo = new GameUnit( "return_to_cockpit", false, unit->faction, "", fg, fgsnumber, NULL);
                        if (unit->owner)
                            cargo->owner = unit->owner;
                        else
                            cargo->owner = unit;
                    } else {
                        int fac = FactionUtil::GetUpgradeFaction();
                        static float ejectcargotime =
                            XMLSupport::parse_float( vs_config->getVariable( "physics", "eject_live_time", "0" ) );
                        if (cargotime == 0.0) {
                            cargo = new GameUnit( "eject", false, fac, "", NULL, 0, NULL);
                        } else {
                            cargo = new Missile( "eject",
                                                               fac, "",
                                                               0,
                                                               0,
                                                               ejectcargotime,
                                                               1,
                                                               1,
                                                               1);
                        }
                    }
                    arot = erot;
                    cargo->PrimeOrders();
                    cargo->aistate = NULL;
                } else {
                    string tmpnam = tmpcontent+".cargo";
                    static std::string nam( "Name" );
                    float  rot    = crot;
                    if (UniverseUtil::LookupUnitStat( tmpnam, "upgrades", nam ).length() == 0) {
                        tmpnam = "generic_cargo";
                        rot    = grot;
                    }
                    int upgrfac = FactionUtil::GetUpgradeFaction();
                    cargo = new Missile( tmpnam.c_str(),
                                                        upgrfac,
                                                        "",
                                                        0,
                                                        0,
                                                        cargotime,
                                                        1,
                                                        1,
                                                        1);
                    arot = rot;
                }
            }
            if (cargo->name == "LOAD_FAILED") {
                cargo->Kill();
                cargo = new Missile( "generic_cargo",
                                                   FactionUtil::GetUpgradeFaction(), "",
                                                   0,
                                                   0,
                                                   cargotime,
                                                   1,
                                                   1,
                                                   1);
                arot = grot;
            }
            Vector rotation( vsrandom.uniformInc( -arot, arot ), vsrandom.uniformInc( -arot, arot ), vsrandom.uniformInc( -arot,
                                                                                                                          arot ) );
            static bool all_rotate_same =
                XMLSupport::parse_bool( vs_config->getVariable( "graphics", "cargo_rotates_at_same_speed", "true" ) );
            if (all_rotate_same && arot != 0) {
                float tmp = rotation.Magnitude();
                if (tmp > .001) {
                    rotation.Scale( 1/tmp );
                    rotation *= arot;
                }
            }
            if ( 0 && cargo->rSize() >= unit->rSize() ) {
                cargo->Kill();
            } else {
                Vector tmpvel = -unit->Velocity;
                if (tmpvel.MagnitudeSquared() < .00001) {
                    tmpvel = randVector( -unit->rSize(), unit->rSize() ).Cast();
                    if (tmpvel.MagnitudeSquared() < .00001)
                        tmpvel = Vector( 1, 1, 1 );
                }
                tmpvel.Normalize();
                if ( (SelectDockPort( unit, unit ) > -1) ) {
                    static float eject_cargo_offset =
                        XMLSupport::parse_float( vs_config->getVariable( "physics", "eject_distance", "20" ) );
                    QVector loc( Transform( unit->GetTransformation(), unit->DockingPortLocations()[0].GetPosition().Cast() ) );
                    //index is always > -1 because it's unsigned.  Lets use the correct terms, -1 in Uint is UINT_MAX
                    loc += tmpvel*1.5*unit->rSize()+randVector( -.5*unit->rSize()+(index == UINT_MAX ? eject_cargo_offset/2 : 0),
                                                         .5*unit->rSize()+(index == UINT_MAX ? eject_cargo_offset : 0) );
                    cargo->SetPosAndCumPos( loc );
                    Vector p, q, r;
                    unit->GetOrientation( p, q, r );
                    cargo->SetOrientation( p, q, r );
                    if (unit->owner)
                        cargo->owner = unit->owner;
                    else
                        cargo->owner = unit;
                } else {
                    cargo->SetPosAndCumPos( unit->Position()+tmpvel*1.5*unit->rSize()+randVector( -.5*unit->rSize(), .5*unit->rSize() ) );
                    cargo->SetAngularVelocity( rotation );
                }
                static float velmul = XMLSupport::parse_float( vs_config->getVariable( "physics", "eject_cargo_speed", "1" ) );
                cargo->SetOwner( unit );
                cargo->SetVelocity( unit->Velocity*velmul+randVector( -.25, .25 ).Cast() );
                cargo->Mass = tmp->mass;
                if (name.length() > 0)
                    cargo->name = name;
                else if (tmp)
                    cargo->name = tmpcontent;
                if (cp && _Universe->numPlayers() == 1) {
                    cargo->SetOwner( NULL );
                    unit->PrimeOrders();
                    cargo->SetTurretAI();
                    cargo->faction = unit->faction;
                    //changes control to that cockpit
                    cp->SetParent( cargo, "", "", unit->Position() );
                    if (tmpcontent == "return_to_cockpit") {
                        static bool simulate_while_at_base =
                            XMLSupport::parse_bool( vs_config->getVariable( "physics", "simulate_while_docked", "false" ) );
                        if ( (simulate_while_at_base) || (_Universe->numPlayers() > 1) ) {
                            unit->TurretFAW();
                        }
                        //make unit a sitting duck in the mean time
                        SwitchUnits( NULL, unit );
                        if (unit->owner) {
                            cargo->owner = unit->owner;
                        } else {
                            cargo->owner = unit;
                        }
                        unit->PrimeOrders();
                        cargo->SetOwner( unit );
                        cargo->Position() = unit->Position();
                        cargo->SetPosAndCumPos( unit->Position() );
                        //claims to be docked, stops speed and taking damage etc. but doesn't seem to call the base script
                        cargo->ForceDock( unit, 0 );
                        abletodock( 3 );
                        //actually calls the interface, meow. yay!
                        cargo->UpgradeInterface( unit );
                        if ( (simulate_while_at_base) || (_Universe->numPlayers() > 1) ) {
                            unit->TurretFAW();
                        }
                    } else {
                        SwitchUnits( NULL, cargo );
                        if (unit->owner) {
                            cargo->owner = unit->owner;
                        }
                        else {
                            cargo->owner = unit;
                        }
                    }                                            //switching NULL gives "dead" ai to the unit I ejected from, by the way.
                }
                _Universe->activeStarSystem()->AddUnit( cargo );
                if ( (unsigned int) index != ( (unsigned int) -1 ) && (unsigned int) index != ( (unsigned int) -2 ) ) {
                    if ( index < unit->pImage->cargo.size() ) {
                        RemoveCargo( index, 1, true );
                    }
                }
            }
        }
    }
}

int Carrier::RemoveCargo( unsigned int i, int quantity, bool eraseZero )
{
    Unit *unit = static_cast<Unit*>(this);
    if ( !( i < unit->pImage->cargo.size() ) ) {
        BOOST_LOG_TRIVIAL(error) << "(previously) FATAL problem...removing cargo that is past the end of array bounds.";
        return 0;
    }
    Cargo *carg = &(unit->pImage->cargo[i]);
    if (quantity > carg->quantity) {
        quantity = carg->quantity;
    }

    static bool usemass = XMLSupport::parse_bool( vs_config->getVariable( "physics", "use_cargo_mass", "true" ) );
    if (usemass) {
        unit->Mass -= quantity*carg->mass;
    }

    carg->quantity -= quantity;
    if (carg->quantity <= 0 && eraseZero) {
        unit->pImage->cargo.erase( unit->pImage->cargo.begin()+i );
    }
    return quantity;
}

void Carrier::AddCargo( const Cargo &carg, bool sort )
{
    Unit *unit = static_cast<Unit*>(this);

    static bool usemass = XMLSupport::parse_bool( vs_config->getVariable( "physics", "use_cargo_mass", "true" ) );
    if (usemass) {
        unit->Mass += carg.quantity*carg.mass;
    }
    unit->pImage->cargo.push_back( carg );
    if (sort) {
        SortCargo();
    }
}

bool cargoIsUpgrade( const Cargo &c )
{
    return c.GetCategory().find( "upgrades" ) == 0;
}

float Carrier::getHiddenCargoVolume() const
{
    const Unit *unit = static_cast<const Unit*>(this);
    return unit->pImage->HiddenCargoVolume;
}

bool Carrier::CanAddCargo( const Cargo &carg ) const
{
    const Unit *unit = static_cast<const Unit*>(this);

    //Always can, in this case (this accounts for some odd precision issues)
    if ( (carg.quantity == 0) || (carg.volume == 0) )
        return true;
    //Test volume availability
    bool  upgradep     = cargoIsUpgrade( carg );
    float total_volume = carg.quantity*carg.volume+( upgradep ? getUpgradeVolume() : getCargoVolume() );
    if ( total_volume <= ( upgradep ? getEmptyUpgradeVolume() : getEmptyCargoVolume() ) )
        return true;
    //Hm... not in main unit... perhaps a subunit can take it
    for (un_kiter i = unit->viewSubUnits(); !i.isDone(); ++i)
        if ( (*i)->CanAddCargo( carg ) )
            return true;
    //Bad luck
    return false;
}

//The cargo volume of this ship when empty.  Max cargo volume.
float Carrier::getEmptyCargoVolume( void ) const
{
    const Unit *unit = static_cast<const Unit*>(this);
    return unit->pImage->CargoVolume;
}

float Carrier::getEmptyUpgradeVolume( void ) const
{
    const Unit *unit = static_cast<const Unit*>(this);
    return unit->pImage->UpgradeVolume;
}

float Carrier::getCargoVolume( void ) const
{
    const Unit *unit = static_cast<const Unit*>(this);
    float result = 0.0;
    for (unsigned int i = 0; i < unit->pImage->cargo.size(); ++i)
        if ( !cargoIsUpgrade( unit->pImage->cargo[i] ) )
            result += unit->pImage->cargo[i].quantity*unit->pImage->cargo[i].volume;
    return result;
}

Unit* Carrier::makeMasterPartList()
{
    unsigned int i;
    static std::string mpl = vs_config->getVariable( "data", "master_part_list", "master_part_list" );
    CSVTable *table = loadCSVTableList(mpl, VSFileSystem::UnknownFile, false);

    Unit *ret = new Unit();
    ret->name = "master_part_list";
    if (table) {
        vsUMap< std::string, int >::const_iterator it;
        for (it = table->rows.begin(); it != table->rows.end(); ++it) {
            CSVRow row( table, it->second );
            Cargo  carg;
            carg.content     = row["file"];
            carg.category    = row["categoryname"];
            carg.volume      = stof( row["volume"], 1 );
            carg.mass        = stof( row["mass"], 1 );
            carg.quantity    = 1;
            carg.price       = stoi( row["price"], 1 );
            carg.description = row["description"];
            ret->GetImageInformation().cargo.push_back( carg );
        }
        delete table;
    }
    UpdateMasterPartList( ret );
    if ( !ret->GetCargo( "Pilot", i ) )     //required items
        ret->AddCargo( Cargo( "Pilot", "Contraband", 800, 1, .01, 1, 1.0, 1.0 ), true );
    if ( !ret->GetCargo( "Hitchhiker", i ) )
        ret->AddCargo( Cargo( "Hitchhiker", "Passengers", 42, 1, .01, 5.0, 1.0, 1.0 ), true );
    if ( !ret->GetCargo( "Slaves", i ) )
        ret->AddCargo( Cargo( "Slaves", "Contraband", 800, 1, .01, 1, 1, 1 ), true );
    return ret;
}

float Carrier::PriceCargo( const std::string &s )
{
    Unit *unit = static_cast<Unit*>(this);

    Cargo tmp;
    tmp.content = s;
    vector< Cargo >::iterator mycargo = std::find( unit->pImage->cargo.begin(),
                                                   unit->pImage->cargo.end(), tmp );
    if ( mycargo == unit->pImage->cargo.end() ) {
        Unit *mpl = getMasterPartList();
        if (this != mpl) {
            return mpl->PriceCargo( s );
        } else {
            static float spacejunk = parse_float( vs_config->getVariable( "cargo", "space_junk_price", "10" ) );
            return spacejunk;
        }
    }
    float price;
    price = (*mycargo).price;
    return price;
}



float Carrier::getUpgradeVolume( void ) const
{
    const Unit *unit = static_cast<const Unit*>(this);
    float result = 0.0;
    for (unsigned int i = 0; i < unit->pImage->cargo.size(); ++i)
        if ( cargoIsUpgrade( unit->pImage->cargo[i] ) )
            result += unit->pImage->cargo[i].quantity*unit->pImage->cargo[i].volume;
    return result;
}



Cargo& Carrier::GetCargo( unsigned int i )
{
    Unit *unit = static_cast<Unit*>(this);
    return unit->pImage->cargo[i];
}

const Cargo& Carrier::GetCargo( unsigned int i ) const
{
    const Unit *unit = static_cast<const Unit*>(this);
    return unit->pImage->cargo[i];
}


void Carrier::GetSortedCargoCat( const std::string &cat, size_t &begin, size_t &end )
{
    Unit *unit = static_cast<Unit*>(this);
    vector< Cargo >::iterator Begin  = unit->pImage->cargo.begin();
    vector< Cargo >::iterator End    = unit->pImage->cargo.end();
    vector< Cargo >::iterator lbound = unit->pImage->cargo.end();
    vector< Cargo >::iterator ubound = unit->pImage->cargo.end();

    Cargo beginningtype;
    beginningtype.category = cat;
    CatCompare Comp;
    lbound = std::lower_bound( Begin, End, beginningtype, Comp );
    beginningtype.content  = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    ubound = std::upper_bound( Begin, End, beginningtype, Comp );
    begin  = lbound-Begin;
    end    = ubound-Begin;
}

// TODO: I removed a superfluous call via this->GetCargo and got a warning about recursion
// all paths through this function will call itself
// The game also crashed due to endless loop.
// I returned the code and now it works and I don't know why.
Cargo* Carrier::GetCargo( const std::string &s, unsigned int &i )
{
    const Unit *unit = static_cast<const Unit*>(this);
    if ( unit->GetCargo( s, i ) )
        return &GetCargo( i );
    return NULL;
}

const Cargo* Carrier::GetCargo( const std::string &s, unsigned int &i ) const
{
    const Unit *unit = static_cast<const Unit*>(this);

    static Hashtable< string, unsigned int, 2047 >index_cache_table;
    Unit *mpl = getMasterPartList();
    if (this == mpl) {
        unsigned int *ind = index_cache_table.Get( s );
        if (ind) {
            if ( *ind < unit->pImage->cargo.size() ) {
                Cargo *guess = &unit->pImage->cargo[*ind];
                if (guess->content == s) {
                    i = *ind;
                    return guess;
                }
            }
        }
        Cargo searchfor;
        searchfor.content = s;
        vector< Cargo >::iterator tmp = std::find( unit->pImage->cargo.begin(),
                                                   unit->pImage->cargo.end(), searchfor );
        if ( tmp == unit->pImage->cargo.end() )
            return NULL;
        if ( (*tmp).content == searchfor.content ) {
            i = ( tmp-unit->pImage->cargo.begin() );
            if (this == mpl) {
                unsigned int *tmp = new unsigned int;
                *tmp = i;
                if ( index_cache_table.Get( s ) )
                    index_cache_table.Delete( s );
                //memory leak--should not be reached though, ever
                index_cache_table.Put( s, tmp );
            }
            return &(*tmp);
        }
        return NULL;
    }
    Cargo searchfor;
    searchfor.content = s;
    vector< Cargo >::iterator tmp = ( std::find( unit->pImage->cargo.begin(),
                                                 unit->pImage->cargo.end(), searchfor ) );
    if ( tmp == unit->pImage->cargo.end() )
        return NULL;
    i = ( tmp-unit->pImage->cargo.begin() );
    return &(*tmp);
}

unsigned int Carrier::numCargo() const
{
    const Unit *unit = static_cast<const Unit*>(this);
    return unit->pImage->cargo.size();
}

std::string Carrier::GetManifest( unsigned int i, Unit *scanningUnit, const Vector &oldspd ) const
{
    const Unit *unit = static_cast<const Unit*>(this);

    ///FIXME somehow mangle string
    string mangled = unit->pImage->cargo[i].content;
    static float scramblingmanifest =
        XMLSupport::parse_float( vs_config->getVariable( "general", "PercentageSpeedChangeToFaultSearch", ".5" ) );
    {
        //Keep inside subblock, otherwice MSVC will throw an error while redefining 'i'
        bool last = true;
        for (string::iterator i = mangled.begin(); i != mangled.end(); ++i) {
            if (last)
                (*i) = toupper( *i );
            last = (*i == ' ' || *i == '_');
        }
    }
    if (unit->CourseDeviation( oldspd, unit->GetVelocity() ) > scramblingmanifest)
        for (string::iterator i = mangled.begin(); i != mangled.end(); ++i)
            (*i) += (rand()%3-1);
    return mangled;
}

bool Carrier::SellCargo( unsigned int i, int quantity, float &creds, Cargo &carg, Unit *buyer )
{
    const Unit *unit = static_cast<const Unit*>(this);

    if (i < 0 || i >= unit->pImage->cargo.size() || !buyer->CanAddCargo( unit->pImage->cargo[i] )
            || unit->Mass < unit->pImage->cargo[i].mass)
        return false;
    carg = unit->pImage->cargo[i];
    if (quantity > unit->pImage->cargo[i].quantity)
        quantity = unit->pImage->cargo[i].quantity;
    carg.price = buyer->PriceCargo( unit->pImage->cargo[i].content );
    creds += quantity*carg.price;

    carg.quantity = quantity;
    buyer->AddCargo( carg );

    RemoveCargo( i, quantity );
    return true;
}

bool Carrier::SellCargo( const std::string &s, int quantity, float &creds, Cargo &carg, Unit *buyer )
{
    const Unit *unit = static_cast<const Unit*>(this);

    Cargo tmp;
    tmp.content = s;
    vector< Cargo >::iterator mycargo = std::find( unit->pImage->cargo.begin(), unit->pImage->cargo.end(), tmp );
    if ( mycargo == unit->pImage->cargo.end() )
        return false;
    return SellCargo( mycargo-unit->pImage->cargo.begin(), quantity, creds, carg, buyer );
}

bool Carrier::BuyCargo( const Cargo &carg, float &creds )
{
    if (!CanAddCargo( carg ) || creds < carg.quantity*carg.price)
        return false;
    AddCargo( carg );
    creds -= carg.quantity*carg.price;

    return true;
}

bool Carrier::BuyCargo( unsigned int i, unsigned int quantity, Unit *seller, float &creds )
{
    Cargo soldcargo = seller->pImage->cargo[i];
    if (quantity > (unsigned int) soldcargo.quantity)
        quantity = soldcargo.quantity;
    if (quantity == 0)
        return false;
    soldcargo.quantity = quantity;
    if ( BuyCargo( soldcargo, creds ) ) {
        seller->RemoveCargo( i, quantity, false );
        return true;
    }
    return false;
}

bool Carrier::BuyCargo( const std::string &cargo, unsigned int quantity, Unit *seller, float &creds )
{
    unsigned int i;
    if ( seller->GetCargo( cargo, i ) )
        return BuyCargo( i, quantity, seller, creds );
    return false;
}
