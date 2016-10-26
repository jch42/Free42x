# Free42x
Free42x is an extension to Thomas Okken's Free42 re-implementation of HP-42S calculator (http://thomasokken.com/free42)
This extension attempts to provide HP-IL capabilities to Free42, maybe as the stillborn HP-42SX could have added ?

Only Windows and Android platforms yet, but most of the HP-IL functions code is held in common sources,
to minimize and ease porting to another platform.

Link to HP-IL devices makes use of either TCP/IP (Virtual HP-IL, look at http://hp.giesselink.com/hpil.htm)
or Serial Com port to a PIL-Box (HP-IL - USB Interface - http://www.jeffcalc.hp41.eu/hpil/index.html).

HP-IL functions already released:

* printer operations:
	All HP-42S print functions are translated to HP-IL 82162A commands.
	Graphic output (PRLCD) suffers from the limitations of the 82162A,
	line spacing prevents smooth graphs editing and columns accumulation limits line contents.
	
* mass storage operations:
	operates as their 82160A HP-IL Module counterparts (and SKWID EXT IL Rom when relevant).
	NEWM (snewm), DIR (sdir), WRTP (swrtp), READP, CREATE (screate), WRTR (swrtr), ZERO, READR, SEC, UNSEC, RENAME, PURGE.
  No multiple mass storage operation, one new function DSKSEL permits the selection of the desired mass storage device.
  Compatibility with HP-41C format kept, when reading and writing unidimensionnal real matrix.
  Two new data file types, one for single HP-42S object storage and another for multiple objects.
	
* interface control operations, with a mix from 82160A HP-IL Module, 82183 HPIL Extended IO Module and SKWID EXT IL Rom:
	SELECT, AUTOIO, MANIO, NLOOP, RCLSEL, ID, AID, STAT, PRTSEL.

* some 82183 HPIL Extended IO Module data transfer functions:
  IN and OUT functions, no Program datatype,
  one new termination mode (Specified character or CR/LF) in order to deal with csv files.
  CLRDEV and CLRLOOP and ANUMDEL.
  ANUMDEL has been enhanced to read complex number.

