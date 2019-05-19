SECTIONS
{
	.flash.rodata :
	{
	    _initcall_simple_start = ABSOLUTE(.);
	    KEEP(*(.initcall_simple))
	    KEEP(*(.initcall_simple.*))
	    _initcall_simple_stop = ABSOLUTE(.);

	   	_cmd_st_simple_start = ABSOLUTE(.);
	    KEEP(*(.cmd_st_simple))
	    KEEP(*(.cmd_st_simple.*))
	    _cmd_st_simple_stop = ABSOLUTE(.);

	   	_evt_hlp_simple_start = ABSOLUTE(.);
	    KEEP(*(.evt_hlp_simple))
	    KEEP(*(.evt_hlp_simple.*))
	    _evt_hlp_simple_start = ABSOLUTE(.);
	 } >drom0_0_seg
}