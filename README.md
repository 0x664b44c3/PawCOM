# PawCOM - a Partyline Analog (Wired) intercom system

An open source intercom system with professional features.

## Abstract
This projects implements a partyline analog intercom system as used in the tv and event industry for interconnecting small teams of technical staff.

The system consists of any number from 2 up of so-called beltpacks, portable intercom stations plus a central power supply.
The portable parts are interconnected by generic microphone cabling (industry standard) and is electrically compatible to ClearCom, RTS and others.
Analog circuitry was inspired by the classic ClearCom MR102 and others but has been re-engineered from the ground up to use modern components and
add a few extra features.


## PAWCOM features

- analog signal processing with digital control
  - latency free
  - compressor/limiter on mic channel
  - limiter in Rx audio path (your ears will love you)
  - digitally adjustable sidetone level
  - adjustable MIC gain
  - low background noise
- Digital volume control
- digital PTT control with both latching and push to talk mode
- switchable mic power for electred mic headsets
- stereo headphone amp
- optional line in for program audio or personal music player

- click/pop free Call function
  - automatic audible call function if call is held for a certain time

## Further reading and other projects about Partyline

You may have a look at the fnordcom project that has compiled a number of schematics and information around
older clearcom intercom systems: https://github.com/pc-coholic/fnordcom

## Versions and known issues

Version 1.0 has two bugs; one being in the feedback path of IC2C, the second being the mic compressor/limiter detector which should be connected to the output of the attenuator switch
but before the transmit switch. It's currently connected to the output of the mic amplifier.
