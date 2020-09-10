TODO:

. Clean old editor... really CLEAN !
. Put back audio engine reinint ! (BACK + MENU)
. Move Gate Fx to regular filters


Bug:


In progress :
. Finish all MENUs


Not tested : 

-----------------------------------------------------
Optional / Nice to have : 
. UI : show LFO/OSC shapes 
. UI : show Env (other envs ???)
. Midi send and receive per instrument in mixer
. Editor OP : display op number in bottom middle empty button (e.g : 1/3)
-----------------------------------------------------


EPIC
. Add Feedback to all ALGO ?
. Add limitter for each stereo DAC



Done :
. 2 destinations per matrix row
. Cpu usage as settings option + tft acces outside audio thread
. Oscilloscope moves when fTines!=0. Use real fresquency Osc.cpp
. Add All/Current instruments midi input in mixer/other menu
. Get rid of MAX_NUMBER_OF_OPERATORS. Uses MAX_NUMBER_OF_VOICES only.
. Update screen when new value from external
. Bug : Reset ('-' + encoder) does not work on OP envelope (For op2 and more)
. Bug : Changing Lvl on OP 2+ calls reloadASDR for OP1.env only.
. Mono(2) plays 2 voices. Should play only 1.
. Display OP oscillator Ftyp=fixe 
. Display lfo frequency : after 99.5 / M/16 etc.. Erase problem.
. Display Lfo Ksyn Off. Display bug.
. Finish midi USB (midi out)
. Includes other Filter from prfm2 forum !
. add filter to perf page
. Bug fix : ALGO 8 : Mix/pan5 in algo is modified by Mix/pan2 on the preenfm2
. Nicer Rand menu 
. Volume (voice/timbre/out) is now OK. Oscillo ok per timbre.
. Put back play note from keyboard
. Remove 2 of the 6 perfs
. Go back to mixer (not edit) after loading mixer or preset
. Boot loader
. Save Combo/Mixer
. Put back Scala per instrument
. Init scala scales after mixer load
. Scala per instrument
. Menu : move back from regular button to isolate one
. get rid of allChars. Use direct chars.
. Use Mixer Range info (Midi first, last, shift)
. Use Number of voice of mixer (prevent inconsistency with Poly/Mono)
. Instrument engine : replace number of voice per Poly/Mono
. Midi channel : add "All" option
. Nicer (little bigger) buttons. More compact chars.
. Midi general settings + Global tuning
. Display refactoring (Mixer, Editor, Menu)
. Create smaller font to display midi activity and midi clock
. Get rid of sysex ?
. UI : show currently edited OP  in the algo
. UI : show IM modified in the algo (1 sec)
. EPIC : Menu button !
. Add all other algos !!
. Remove SynthParamChecker.h
. Colors : Mixer in Green, Editor in blue, Menu in Red
. Editor => OP (Osc) : Values are not correct
. Make editor value Yellow when changing (like in mixer)
. Encoder : Add longPress ! (0.5 secs)
. Editor : long press go back to main editor page
. Display Note for Midi clock
. Bug modified preset "*" wrong background

