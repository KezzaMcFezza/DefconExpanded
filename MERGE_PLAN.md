# Merge Plan: THUNDYv2 → DefconExpanded

**Source**: `c:\Users\miles\DEFCONPROJECT\THUNDYv2`  
**Target**: `C:\DefconExpanded` (all edits here only)

**Unit stats reference** (goatcon): [Google Sheets](https://docs.google.com/spreadsheets/d/1-H62mTA2QfVusg6o__63ntxi6qa7lSKGxBhz7t7S-lM/edit?gid=1935469833#gid=1935469833) — Life Points, Stealth Type, Speed, Turn Rate, Prepare/Reload Time, Action/Fuel Range, Nuke/LACM supply, Max Fighters, per-airbase launch options

Territory-specific aircraft launcher loads

Generic
  Airbase: fighter_light 2, fighter 2
  Carrier: fighter 2

USA:
  Airbase: fighter_light 5, fighter_navy_stealth 2, AEW 2
  Airbase2: fighter_stealth 2, bomber_fast 2, bomber_stealth 2, bomber 5, AEW 2
  Airbase3: fighter 2, fighter_navy_stealth 2, AEW 2
  Carrier_super: fighter 4, fighter_navy_stealth 2, AEW 2
  Carrier: fighter_navy_stealth 2

NATO
  Airbase: fighter 5, AEW 2
  Airbase2: fighter_light 2, fighter_navy_stealth 2, AEW 2
  Carrier: fighter 3, AEW 1
  Carrier_light: fighter_navy_stealth 3

Japan:
  Airbase: fighter_light 2, fighter_navy_stealth 2, AEW 2
  Carrier: fighter_navy_stealth 2

Korea:
  Airbase: fighter_light 2, fighter_navy_stealth 2, AEW 2

Taiwan:
  Airbase: fighter_light 5, AEW

Ukraine:
  Airbase: fighter_light 5, fighter 2

Australia:
  Airbase: fighter 5, fighter_navy_stealth 2, AEW 2
  Carrier: fighter_navy_stealth 2

Russia:
  Airbase: fighter_light 2, bomber_fast 3, bomber 2, AEW 2
  Airbase2: fighter 5, fighter_stealth 2, AEW 2
  Carrier: fighter 3

China:
  Airbase: fighter 5, fighter_stealth 2, bomber 5, AEW 2
  Airbase2: fighter_light 5, fighter 5, fighter_navy_stealth 2
  Carrier_super: fighter 3, fighter_navy_stealth 2, AEW 2
  carrier: fighter 3
  carrier_light: fighter 3

India:
  Airbase: fighter_light 5, fighter 2, AEW 2
  Carrier: fighter 3

Egypt:
  Airbase: fighter_light 5, fighter 2, AEW 2

Saudi:
  Airbase: fighter 3, AEW 2

Israel:
  Airbase: fighter_light 5, figher_navy_stealth 2, AEW 2

NorthKorea:
Indonesia:
Pakistan:
Iran:
  Airbase: fighter_light 2, fighter 2

Terrutory-based unit graphics list:

generic
use the given bmp in the unit constructor

USA: uses generic
China:
  fighter_light: j10
  fighter: flanker
  fighter_navy_stealth: j35
  fighter_stealth: j20
  bomber: h6
  battleship: Type55
  battleship_ddg: Type52D
  battleship_fg: Type54A
  carrier_super: Fujian
  carrier: Kuznetsov
  carrier_light: Kuznetsov
  carrier_lhd: Type075
Russia:
  fighter_light: mig29
  fighter: flanker
  fighter_stealth: su57
  bomber: tu95
  bomber_fast: tu22m
  battleship: slava
  battleship_ddg: udaloy
  battleship_fg: admiralgrigorovich
  carrier: Kuznetsov
  carrier_light: Kuznetsov
NATO:
  fighter: rafale
  battleship_ddg: horizon
  battleship_fg: fremm
  carrier: degaulle
  carrier_light: queen
India:
  fighter: rafale
  fighter_light: flanker
  carrier: Kuznetsov
Pakistan:
  fighter_light: j10
JAPAN:
  carrier: Izumo
Ukraine:
  fighter_light: mig29
  fighter: flanker
Indonesia:
  fighter: flanker
NorthKorea:
  fighter_light: mig29
  fighter: flanker
Iran:
  fighter_light: mig29
  fighter: f14
Saudi:
  fighter: rafale
Egypt
  fighter: rafale
Vietnam:
  fighter: flanker
Neutrals
  fighter: flanker




expansion of unit system                                - INPROGRESS
  missiles - nuke, ballistic, lacm                          - DONE
  gunfire - gunfire (antiair), torpedo, depthcharge, antibm - DONE
  silos - icbm, mrbm, tel-n, tel-c, ascm                    - DONE
  SAMs - abm, sam                                           - DONE
  airbase - 1,2,3                                           -
  radar - radar, radarEW                                    -
  ships - cruiser, destroyer, frigate                       -
  (battleship = cruiser, battleship_ddg = destroyer, battleship_fg = frigate)
  carriers - nuclear, escort                                -
  (carrier, carriersuper, carrierlight)
  subs - ssbn, ssg, ssn, ssk                                -
  (sub = ssbn, subg = ssg, subc = ssn, subk = ssk)
  bombers - normal, stealth, fast                           -
  fighters - naval, stealth naval, light, stealth           -
  AEW - AWACS                                               -

TerritoryUSA:

row 1: Airbase, Silo
row 2: Airbase2, ASCM
row 3: Airbase3, 
row 4: RadarEW, SAM
row 5: Radar, ABM

row 1: carriersuper
row 2: carrierlight
row 3: battleship, sub
row 4: battleship_ddg, subg
row 5: battleship_fg, subc

TerritoryNATO

row 1: Airbase, 
row 2: Airbase2, ASCM
row 3: RadarEW, SAM
row 4: Radar, ABM

row 1: carrier
row 2: carrierlight
row 3: empty, sub
row 4: battleship_ddg, subc
row 5: battleship_fg, subk

TerritoryRussia

row 1: Airbase, Airbase2
row 2: Silo, SiloMed
row 3: SiloMobile, SiloMobileCon 
row 4: RadarEW, Radar
row 5: SAM, ABM
row 6: ASCM

row 1: carrier, sub
row 2: battleship, subg
row 3: battleship_ddg, subc
row 4: battleship_fg, subk

TerritoryChina

row 1: Airbase, Airbase2
row 2: Silo, SiloMed
row 3: SiloMobile, SiloMobileCon 
row 4: RadarEW, Radar
row 5: SAM, ABM
row 6: ASCM

row 1: carriersuper
row 2: carrier, carrierlight
row 3: battleship, sub
row 4: battleship_ddg, subg
row 5: battleship_fg, subc
row 6: carrierLHD, subk

TerritoryIndia

row 1: Airbase
row 2: SiloMed
row 3: SiloMobile, SiloMobileCon
row 4: SAM, ASCM
row 5: Radar

row 1: carrier
row 2: battleship_ddg
row 3: battleship_fg
row 4: sub
row 5: subc
row 6: subk

TerritoryPakistan

row 1: Airbase
row 2: SiloMed
row 3: SiloMobileCon
row 4: ASCM
row 5: SAM
row 6: Radar

row 1: battleship_ddg
row 2: battleship_fg
row 3: subk

TerritoryJapan

row 1: Airbase
row 2: ABM
row 3: SAM
row 4: ASCM
row 5: RadarEW
row 6: Radar

row 1: carrierlight
row 2: battleship_ddg
row 3: battleship_fg
row 4: subk

TerritoryKorea

row 1: Airbase
row 2: SiloMobileCon
row 3: ASCM
row 4: SAM
row 5: ABM
row 6: RadarEW, Radar

row 1: battleship_ddg
row 2: battleship_fg
row 3: subk

TerritoryAustralia

row 1: Airbase
row 2: ASCM
row 3: SAM
row 4: ABM
row 5: RadarEW
row 6: Radar

row 1: carrierlight
row 2: battleship_ddg
row 3: battleship_fg
row 4: subc
row 5: subk

unfinished:
Taiwan
Installations
● Airbase (ROC)
● Conventional TEL / ASBM
● SAM
● ABM
● Radar
● Early Warning Radar
● Coastal ASCM Battery
Air Assets
● AEW Aircraft
● F-16 Viper
Naval Assets
● Generic Destroyer
● Generic Frigate
● SSK

Ukraine
Installations
● Airbase (Ukraine)
● SAM
● ABM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27 Flanker (family abstraction)

Philippines
Installations
● Generic Airbase (Western)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
Naval Assets
● Generic Frigate

Thailand
Installations
● Generic Airbase (Western)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
Naval Assets
● Generic Frigate

Indonesia
Installations
● Airbase (Indonesia)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27/30 Flanker (family abstraction)
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

NorthKorea
Installations
● Generic Airbase (Eastern)
● Silo Medium (MRBM)
● Mobile Launcher (Nuclear TEL)
● Conventional TEL / ASBM
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● Su-27 Flanker (family abstraction)
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● SSK

Islamic Republic of Iran
Installations
● Airbase (Iran)
● Silo Medium (MRBM)
● Conventional TEL / ASBM
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-14A Tomcat
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● SSK

Israel
Installations
● Airbase (Israel)
● Silo (ICBM)
● Conventional TEL / ASBM
● SAM
● ABM
● Radar
● Early Warning Radar
● Coastal ASCM Battery
Air Assets
● AEW Aircraft
● F-16 Viper
● F-35 Lightning II
Naval Assets
● Generic Frigate
● SSK

Saudi Arabia, Bahrain, and Qatar
Installations
● Airbase (Saudi Arabia)
● Conventional TEL / ASBM
● SAM
● ABM
● Radar
● Early Warning Radar
● Coastal ASCM Battery
Air Assets
● AEW Aircraft
● Rafale
● F-16 Viper
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

Egypt
Installations
● Airbase (Egypt)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● AEW Aircraft
● F-16 Viper
● Rafale
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

Vietnam
Installations
● Generic Airbase (Eastern)
● Conventional TEL / ASBM
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● Su-27/30 Flanker (family abstraction)
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK
Brazil
Installations
● Generic Airbase (Western)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper (generic Western fighter abstraction)
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

South Africa
Installations
● Generic Airbase (Western)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper (generic Western fighter abstraction)
Naval Assets
● Generic Frigate
● SSK

Neutral Americas
Installations
● Generic Airbase (Western)
● Generic Airbase (Eastern)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27/30 Flanker (family abstraction)
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

Neutral Africa
Installations
● Generic Airbase (Western)
● Generic Airbase (Eastern)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27/30 Flanker (family abstraction)
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

Neutral Europe
Installations
● Generic Airbase (Western)
● Generic Airbase (Eastern)
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27/30 Flanker (family abstraction)
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

Neutral Asia
Installations
● Generic Airbase (Western)
● Generic Airbase (Eastern)
● Conventional TEL / ASBM
● SAM
● Radar
● Coastal ASCM Battery
Air Assets
● F-16 Viper
● Su-27/30 Flanker (family abstraction)
● MiG-29 Fulcrum
Naval Assets
● Generic Frigate
● Generic Destroyer
● SSK

