-l rts30.lib 

flashmain.obj

-o flash.out
-m flash.map

MEMORY
{  

    RAM1 : org = 0x800000, len = 0x4000
}    
   
SECTIONS
{
      .cinit  : {} >RAM1     
      .stack  : {} >RAM1
      .text   : {flashmain.obj(.text)} >RAM1
      .const  : {} >RAM1
      .vect   : {} >RAM1
      .bss    : {} >RAM1  
      .variable: {} >RAM1
      
}  