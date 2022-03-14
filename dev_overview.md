---
title: RP2040 Doom
description: Development Overview
---

This is part of the series behind the scenes of RP2040 Doom:

* [Introduction](index.md)
* [Rendering And Display Composition](rendering.md)
* [Making It All Fit In Flash](flash.md)
* [Making It Run Fast And Fit in RAM](speed_and_ram.md)
* [Music And Sound](sound.md)
* [Network Games](networking.md) 
* Development Overview **<- this part**

See [here](https://www.youtube.com/playlist?list=PL-_wCtHUfdDPi7i-4OIy5iQjQ3QSqq1Mh) for some nice videos of
RP2040 Doom in action. The code is [here](https://github.com/kilograham/rp2040-doom).

## Overview

The following is a brief high level overview of the development steps, with some details of things that aren't 
described in other sections. I am apparently incapable of using meaningful commit messages, so I include some 
examples here as a window into my sanity/thinking at any given point!

## Step 1 - Build wrangling and new Doom executables

* Refactored the `CMake` build to make it easier to add additional Doom executables with different build 
  settings.

* Added a new *doom_minimal* executable with large portions of the code `#ifdef`-ed out. Whilst this executable is no 
  longer present (I broke it somewhere along the way) the `#defines` used are still present in the codebase, e.g. 
  `NO_USE_NET`, `NO_USE_MOUSE`, 
  `NO_USE_WIPE`, `NO_USE_LOAD`, `NO_USE_SAVE` etc.

* Added Raspberry Pi Pico SDK support for *doom_minimal* 
  alongside the 
  regular native build. The SDK is very code-porting friendly, as it supports building code targeting both the 
  device, but also for the native host system. It is therefore easy to debug code on the host system that is just 
  generally still too 
  big, or 
  too festooned with debug code or `printf`s, to target the actual RP2040 device. Indeed,
  at this point in time, just the `.bss` of `doom_minimal` was bigger than the entire RAM of the RP2040.

  Additionally, the `pico_scanvideo` 
  and `pico_audio_` libraries used for video and audio, also work in *host* mode using SDL2, which is invaluable for 
  debugging, and indeed, RP2040 Doom remains playable under SDL2 to this day!

* Switched *doom_minimal* to use `memmap`, and threaded `const` through the code to ensure the WAD is never written to. 

Random commits: *"ooh mmap"*, *"what is vanilla doom I wonder?"*

## Step 2 - Proving out the new 3D rendering strategy

* A new `pd_` (Pico Doom) rendering variant was prototyped. The idea was to build up display-lists of columns 
  during the rendering pass that could be drawn to the display later. The columns would be z-sorted and clipped to 
  each other, producing a no-overdraw covering of the screen.

* The  `pd_` rendering was used in place of the original rendering in the `doom_minimal` executable, with a 
  *column-to-framebuffer* back end. Considerable
  work went in during this phase to prove that the rendering worked correctly all throughout the game, fixing nasty 
  edge cases along the 
  way.

* Statistics were added about the rendering, number of textures source pixels touched, and dozens of other metrics to 
  guide the choice of final rendering technique, and whether frame-buffers would be required (read more about 
  this in the 
  rendering section [here](rendering.md)).

Random commits: *"look ma no visplanes"*, *"oops broke something"*

## Step 3 - Prototyping ahead-of-the-beam rendering

* Due to perceived frame-buffer memory constraints, the plan was still to attempt to render the scene at 60fps 
  ahead-of-the-beam from the column display-lists. This was prototyped in a new standalone executable called 
  *"baby"* for a reason I don't really recall (I guess it was a small first step).

* Rendering was based on canned display lists, dumped as C arrays from *doom_minimal* when a special key 
  was pressed.

Random commits; *"hot fuzz"*, *"confused"*,
*"pixels - gloriously crappy pixels"*, *"wootling - purple sky at night; textured delight"*

## Step 4 - Getting ahead-of-the-beam rendering working on RP2040 adding floors/ceilings

* The *"baby"* rendering had thus far just been doing sprites and walls. Support was added for filling the floor 
  textures which is done horizontally, after the vertical bits.

* Worked on the RP2040 port of *"baby"* with assembly code, and heavy use of the RP2040 interpolators, eeking out 
  every last bit of performance to get the full 
  ahead-of-the-beam rendering working on one overclocked core at 60fps.
  
Random commits: *"split baby in 3"*, *"full on tee fucking hee"*

## Step 5 - Working towards a real RP2040 Doom binary

* Having proved that drawing a frame on the device at 60fps was possible, it was time to try and get that working in 
the context of actual gameplay. 

* `doom_minimal` my test-bed application was too big to fit on the device, but still served a useful purpose, so 
  a new executable `doom_tiny` was made with even further constraints. Additionally `doom_tiny` would be the first 
  to use embedded WAD data, as there is no filesystem on the RP2040. `doom_tiny` is the executable that is RP2040 
  Doom today. 

* A new `lump_filter` executable was added to remove unused (as of yet) lumps of data from the WAD as the whole WAD 
  would 
  clearly not fit. Initially 
  debug 
  output from 
  `doom_minimal` was used to indicate which lumps were needed on the happy path.

* I discovered that Chocolate Doom converts the `MUS` music into `MIDI` on the fly at runtime for use. This was 
  not going to work in the long run, so I modified `doom_minimal` to dump out the new binary MIDI representation, 
  for replacement in the embedded WAD data via `lump_filter`.

* With this and some more work, `doom_tiny` with almost everything else hacked out, 
  was able to compile for the RP2040, make it through the initialization code and start playing some music. It was of 
  course 
  painfully 
  slow though.

Random commits: *"argggh midi conversion is nuts.. and loading music from temp file; jeez"*, *"somehow doomy_tiny 
makes some noise"*

## Step 6 - A diversion into OPL2 emulation

* I wanted real Doom music, and clearly the music generation (emulation of the Yamaha YM3812 - OPL2 chip) was going 
  to be speed, and indeed memory constrained on the device.

* Still, dealing with the music seemed like a less daunting starting point than either making all the level-data and 
  graphics fit, or properly hooking up the 3D rendering, so I decided to start with the music first.

* More details can be found in the sound section [here](sound.md), however a considerable amount of effort went it 
  to choosing, and then heavily rewriting an OPL2 emulator. Initially the overall methodology was changed and 
  the code was rewritten in C++. Subsequently, large parts of the code were rewritten in assembly. Overall a 
  speed-up 30x+ was realized.
  
Random commits: *"good luck!"*, *"arse"*, *"hee haw some sound - although it takes a whole f-ing cpu"*, 
 *"asm bound to work!"*, *"WEEEEEEEE"*

## Step 7 - Getting the game loop running 

* The original code base copies level data from the WAD into RAM, and then runs the level from RAM. I do not have 
  enough 
  RAM for this, so I modified the code via macros to run directly off the slightly different WAD structure which are 
  fortunately little-endian at least.

* With this done, and some of the inconveniently partially-mutable data types split into separate 
  immutable/mutable parts, 
  the 
  game loop was 
  able to run on the RP2040, at least according to `printf`s, and plausible looking per frame times.

Random commits: *"yikes mucho memory"*, *"ok so far"*, *"doh broke something"*

## Step 8 - Adding sound effects

* To get a better sense that the game was running properly, given that there was still no rendering of any sort, I 
  decided to add back the sound effects.

* To save space in flash, the sound effects are compressed to 4-bit per sample via ADPCM.

* I rolled my own multi-channel sound effect mixer, to mix and position the 8 channels of sound effects into the final 
  stereo output buffer.

Random commits: *"shrinky sound effects"*, *"painfulling our way forward with picomixer"*

## Step 9 - Running the *"baby"* prototype alongside the game loop 

* At this point, `doom_tiny` ran on the device, and would run the demos with sound effects and music, but no rendering.

* It was time to marry the *"baby"* ahead-of-the-beam rendering prototype code with the game executable to prove 
  that both could run at once. 

* Lots of core/IRQ and RAM/flash shuffling later, the *baby* rendering was incorporated, complete with its canned 
  column
  lists,
  running along-side the rest of the Doom code. You can see a video of this, with 60fps floor/ceiling 
  scrolling for effect [here](https://www.youtube.com/watch?v=cG7bsLvQKjM)

Random commits: *"doom_baby is born... looks like some flash contention"*, *"oops"*

## Step 10 - Working towards actual Doom rendering, and a new WAD conversion tool

* I had a prototype display mechanism, however no way to generate the column lists in `doom_tiny` as there are no 
  textures in the WHD. Indeed, at this point, I didn't have even the basic metadata about textures sizes, etc. that 
  are 
  required to make it successfully through the rendering code even without drawing anything.

* This was the first point at which I really wanted to change the format of data within the WAD; specifically, I 
  wanted to disentangle the texture metadata, but also temporarily add some 
  "average" color values for each texture, so I could draw something that was at least a little meaningful.

* I had always known that customizing the WAD would eventually be necessary since most things would need to be 
  compressed, and so `whd_gen` was born, along with the new WHD ("Where's Half the Data") format.

* With `whd_gen` in place, `#ifdef`s were threaded throughout the Doom code to start handling WHD data differently.

Random commits: *"woot ... whd loading"*, *"ok wad size is looking a little depressing right now"*

## Step 11 - Adding single-color 3D rendering

* Doom rendering was hooked up again using the `pd_` rendering code path, and column-lists were generated with each 
  correctly-lit column just having a single texture color.

* Rendering was hooked up both to the *"baby"* ahead-of-the beam renderer, but also a single-buffered frame-buffer 
  based render for testing.

* This all resulted in plausible untextured walls, and rectangular block for sprites, so I added new compact 
  version of the patch metadata to the WHD,  describing the 
  runs of opaque/transparent pixels within a graphic.

* This all ended up with a *dangerously* playable game, you can see [here](https://www.youtube.com/watch?
  v=5fthRBt_sPk). Note, you can tell this video is using the single-buffered render from the visible tearing, and 
  also that the sprite, ceiling and floor colors, unlike the wall textures, are random. 

Random commits: *"pink eye"*, *"yes its playable"*, *"really quite fun*"

## Step 12 - A quest to reduce RAM

* I had a sort-of-working game, but I was pretty much out of RAM even without storing the textures for 
  the 
  set of on-screen columns needed by the ahead-of-the-beam display rendering. I really needed to reduce the RAM usage!

* This work is discussed in detail in the [section on memory](speed_and_ram.md).

Random commits: *"getting in on the short pointers"*, *"ok but why"*, *"oops this broke it"*. 

## Step 13 - Working on level data compression

* Having shrunk the RAM usage again, to make room for some texture data in RAM, I still had a problem in that there 
  was no space in flash for the texture data either. The flash was entirely filled by just the level data 
  and no graphics.

* Therefore, it seemed like a good time to start compressing the level data. This work is discussed in detail in the [section on minimzing flash usage](flash.md).

* Frequently, subtle changes made during this time would cause "desync" of demo playback (if you recall demos are 
  just recorded input events), so there was quite a lot of painful debugging of regressions done.

Random commits: *"wiffling"*, *"convertopants"*, *"looks plausible"*

## Step 14 - A foray into music compression, demo compression, transparent walls and cheat codes.

* The music was also taking up a lot of space in the WHD, so with my compression suit already donned, I figured 
  I'd jump in there too. This work is also discussed in detail in the [section on minimzing flash usage](flash.md).

* Whilst playing the "single color" game to "test" the music, I realized that I wasn't correctly rendering walls with 
  *holes*, 
  so I added new transparent texture metadata to the WHD independent of the actual texture pixels which I didn't 
  have yet. This actually made technical sense, as the pixels and transparency information are needed at different 
  points of rendering.

* Additionally, the cheats such as `idms` and `idclev` were useful for testing different music, so I added cheat 
  support back in.  

* Demo compression was also added at this time to save more flash space.

Random commits: *"brain strain"*, *"can this be true?"*, *"mind the gap"*, *"woot decodoplasty"*

## Step 15 - Dragged kicking and screaming into graphic compression

* I had procrastinated far enough. At this point, the textures/sprites would have to be added, but space was limited 
  and they would need to be compressed.

* I needed to contemplate, and calculate the sizes of various compression techniques, before I bothered to write the 
  runtime code to actually decode/render them, since, until I knew things would fit, there was little point.

* Once again, the [section on minimzing flash usage](flash.md) describes a lot of the results of this in detail, but 
  this whole process was rather long, disheartening, and fraught with repeated cases of me thinking I had done 
  something wonderful, only to realize I had made a stupid mistake. 
  
Random commits: *"yup that's broken"*, *"wtf"*, *"double wtf"*, *"fuck frankly"*

## Step 16 - Threading textures all the way through to the rendering and letting the *"baby"* die

* Armed with compressed textures, sprites, and "flats" (used for floors and ceilings) it was time to feed all that 
  data into the column lists, and all the way through to the rendering.

* It was at about this time, that I realized that I would blow out the intended size of my "column" structure in the 
  display-lists if I was to hope to track which source texture strips were needed from frame to frame. Given that, 
  and the daunting coding task of actually managing/compacting these texture strips as they were shared between 
  subsequent frames, I decided that maybe the whole *"baby"* ahead-of-the-beam rendering was a bad idea after all.

* Thus, a new double-buffered frame-buffer `pd_` render *"new hope"* was born, but still using the same column-lists 
  data. 
  Given the 
  previous 
  RAM savings, there was now enough room for the two frame-buffers, if I only kept the top 168 rows in each, 
  i.e. the area 
  above 
  the status bar.

* One benefit of moving back to frame-buffers, is that I could immediately, 
  `#ifdef` the "automap" rendering back in, which I did for a cheap, but satisfying, win.

Random commits: *"extract a few functions"*, *"cogitate"*

## Step 17 - Actually rendering textures from the compressed data

* This was a doozy, both in terms of the complication involved (especially for "composite" textures), but also in 
  terms of juggling speed, use of both cores, and uses of stack vs static RAM scratch space.

* There was quite a bit of round-tripping with `whd_gen` as I realized I'd overlooked various corner cases.

* I also had a bit of a back and forth with myself, over whether to split column fragments from a composite texture 
  column 
  into yet more fragments containing runs of pixel only from a single patch. It turned out that the sheer number of 
  fragments produced, and complications with the fact that textures - unlike sprites - can wrap vertically, scarpered 
  this extra "splitting" approach. 

Random commits: *"um hail mary update of texutremids"*, *"why are there holes?"*, *"stupid duplicate patch"*

## Step 18 - Adding the status bar and menus

* As discussed in the [rendering section](rendering.md), in RP2040 Doom, the status bar needs to be drawn as an 
  overlay using special "V-Patch" graphics.

* This required a new `whd_gen` provided graphic encoding, and the ability to render these patches as an overlay 
  ahead-of-the-beam. This timing was very tight, and the status bar flickered a lot for a good while.

* The same "V-patch" overlays can be used for menus, but these must also be renderable directly into the 
  frame-buffer, so this rendering code was added.

Random commits: *"well that was confusing!"*, *"hmm"*, *"man this status bar is hard"*

## Step 19 - Adding splash and intermission screens

* This was of course easier now that I was using frame-buffers, however I needed a 320x200 frame-buffer not the 320x168 
  size that I had for double-buffering.

* This is discussed in the [rendering section](rendering.md), but there was a bunch of juggling of 
  frame-buffer memory, scanline rendering and V-Patch overlays required to get everything to work nicely.

Random commits: *"something approaching an intermission screen at least working on 
device... very farty music"*, *"tee hee menu during splash"*

## Step 20 - Adding text mode for quit to DOS text

* I thought I was going to use the text mode for the networking menu too, honest!

Random commits: *"woot, that works"*, *"quittastic"*

## Step 21 - Improving texture rendering speed and adding an FPS indicator

* I was still unhappy with performance, so I aded an FPS indicator to track progress. This is still available 
  via the `\` key. In extreme cases we were getting down to about 13fps, so more clearly more needed to be done. 

* I allowed more rendering work to be done on core 1.

* I fixed a stupid bug where traversed but not visible floor/ceiling textures were being decompressed unnecessarily.

* I fixed some nasty edge cases.

Random commits: *"seems less crashy"*.

## Step 22 - Adding wipe effect

* IMHO Doom isn't Doom without the wipe effect, so I added this. Once again, this is discussed in the 
  [rendering section](rendering.md) along with some nice videos.

Random commits: *"ooh wipey"*

## Step 23 - Adding "Bunny" end screen, DOS shell, HELP screens

* I was determined not to hack anything significant out of RP2040 Doom, so I couldn't skimp on the end screens either!

* ... and having made a text mode, I might as well add an interactive DOS prompt (yes I was bored).

* Hacked away more at WHD, code and flash data sizes to squeeze the two 320x200 help screens, the last remaining large 
  items, in.

Random commits: *"saved about 20k i think"*, *"more foolishness, but it does say how much space there isn't"*

## Step 24 - Adding "switches" and texture/flat animations

* I had forgotten about the ability to change the wall textures used when a switch is flicked. This required 
  separate work, as thse defintions are immutable in flash.

* I had forgotten about the ability to animate floors, ceilings and the x-offset of textures, so I fixed this too.

Random commits: *"woot switches for the bitches"*

## Step 25 - Getting more speed-ups

* To speed the more pathological rendering cases up further, I introduced new automatic composite texture 
  optimization in `whd_gen` 
  which 
  allowed 
  faster 
  rendering of textures, 
  particularly when the same patch is repeated in a texture column.

* I added a bunch of small decompression/decoding related caches in the rendering paths.

Random commits: *"wtf this seems slower"*, *"ok new encoding doesn't break shit, lets try a few"*, *"thought stone 
was the problem, it was me"*

## Step 26 - Supporting load/save of games, saving more space

* The existing saved game format needed to be compressed by a factor of about ten. Fortunately this 
  was achievable as a 
  *diff* 
  against the original level state.

* I had to do a bunch of on device work to deal with saving games:
  
  The saved games are written to flash, and so neither core can
  be accessing data or code from the flash when this happens. Sadly, writing the saved game will take multiple 60fps 
  frames, so I either needed to blank the display, which I tried and looked awful, or make sure all the requisite 
  code to 
  keep the 
  display alive is 
  in RAM. I was able to tease enough code out to just display the current framebuffer 
  state 
  without 
  any overlays, and fit that into RAM.

  There was not enough RAM to keep sound running, so I added code to fade the sound out before the save, and back in 
  after, which sounds fine.

* Even with 10x compression, there was no guarantee that six saved games would fit in the small amount of flash space, 
  so I 
  added new logic and 
  popup 
  messages to 
  handle 
  the cases where there isn't enough flash to write to a saved game slot.

* Still a bit unhappy the amount of flash available for saved games, I made another pass at the static read-only data 
  and 
  WHD to 
  free up enough space for 
  what should be 5 
  or 6 slots for any level.

Random commits; *"getting smaller"*, *"somewhat plausible"*

## Step 27 - Adding networking support

* As ever, I did the easy bit first, adding a new set of menus/graphics for in-game networking 
  setup. Chocolate Doom (and I think vanilla Doom) have you decide up front via command line args whether you are 
  launching 
  or joining a network game, or whether you are playing standalone. I wanted all this to be selectable from the 
  running 
  executable, hence the new menus, but this also required some handling of new edge cases expose by the 
  new no-restart-required workflow. 
  
* A completely new I2C networking stack and game networking state machine had to be written, and is 
  discussed in the [section on networking](networking.md).

Random commits: *"ok.. lets try and hail mary the lobby"*, *"ack this is hard"*

## Step 28 - Doing more optimization, code shrinking and bug fixes

* Can never have enough of any of these!

Random commits: *"fun fun good"*

## Step 29 - Adding Ultimate Doom and Doom II support

* Spent time dealing with edge case bugs, and limit overflows, demo "desync" issues and more that these games exposed.

* Added a new `whd_gen` option, and a different `doom_tiny` executable using the less compressed WHD format for these 
  games. 

* Did more RAM hatcheting to make room to cope with some of the new levels which were much larger than those found in 
  `DOOM1.
  WAD`. 

* Added graceful rendering degredation for rare cases where complex level geometry and large numbers of sprites 
  result in too many visible column fragments to render in one frame. 

* Discovered a nasty buffer overflow, requiring the addition of a new decoder size-required field to compressed patch 
  data in 
  the WHD. This actually helped with runtime performance though.
  
Random commits: *"going quite nicely; e4m2 has MANY columns"*, *"remove WEEBLER"*

## Step 30 - Back-filling missing functionality, adding USB support

* Added "gamma correction" support.

* Fixed "ultra" speed override for the *Nighmare* skill level, which had been a casualty of making such settings 
  part of 
  read-only data in flash.  

* Added TinyUSB support, as USB input is nice vs forwarding from a PC over UART. Sadly this costs 8K of flash and 2K of 
  RAM though!

* Combined the `malloc`, and Doom "zone memory" heaps as a fix for this extra 2K loss breaking the ability to play 
  E1M5 on the *Nightmare* skill level. 

Random commits: *"aok"*

## Step 31 - Having a look at "TNT: Evilution" and "The Plutonia Experiment"

* These require a few small technical changes dealing with extra wide textures, and various other techniques 
  employed to create the games with no modifications to the existing Doom code.

* The *TNT: Eviluton* WHD is too big to fit in 8M with the same relaxed `whd_gen` settings as used by *Ultimate 
  Doom* and *Doom II*.

* This and the increased level complexity, indicate that I would need to put in some additional effort to make all the 
  levels of these 
  seamlessly and successfully playable on all levels, so I decided to defer these for now.
  
Random commits: *"arghh.. tex/flat anims don't work they way i thought they did!"*

## Step 32 - Adding the "Doom Cast" end screen

* This got left to the end, as I forgot what game it was in. It's in *Doom II*. Anyways, I added it for 
  completeness; it is discussed in the [section on rendering](rendering.md). 

Random commits: *"ha ok somewhat drawy"*

---

THE END. How about going back to the [Introduction](index.md)?