SECTIONS
{
	.simple :
	{
	    _initcall_simple_start = ABSOLUTE(.);
	    KEEP(*(.initcall_simple))
	    KEEP(*(.initcall_simple.*))
	    _initcall_simple_stop = ABSOLUTE(.);
	 } >drom0_0_seg
}