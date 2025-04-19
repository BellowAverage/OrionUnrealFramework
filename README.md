# Introduction
Orion is an improved & enhanced Top-down game template for Unreal Engine C++ that features advanced AI logics and controls.\
This work is still in progress.

# Inheritance Of Playable Character
Character.h -> OrionChara.h -> OrionCharacter (Blueprint Class Starts here) -> BP_XOrionCharacter -> BP_Character_Custom -> BP_XANDRACustomChar -> BP_XChara

## Gameplay Logic Level
Classes that define character gameplay logics and interact with other modules of the game such as Player/AI Controllers.

### Character.h
Preset C++ character class defined by Unreal.

### OrionChara.h
C++ class inherited from Character.h. Contains major gameplay logics and defines major enums and structs used by Playerable Character.

### OrionCharacter
Blueprint class that defines gameplay logics using blueprints. Nothing is here so far, but is created preparing potential blueprints functions implementations.

## Appearance Customization Level

### BP_XOrionChacter
Blueprint class that defines Character Customization logics.

### BP_Character_Custom & BP_XANDRACustomChar (Better Not Touch)
Blueprint classes created by third-party template provider Xandra. Utilized to define character creation logics.

### BP_XChara
Implementation Level.