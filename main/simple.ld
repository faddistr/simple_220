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
	 } >drom0_0_seg
}