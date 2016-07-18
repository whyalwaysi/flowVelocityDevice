		.text  
		.global _c_int00,_int00,_tint1

reset   br  _c_int00 
        
         .ref    _c_int03
		       ;reference all interrupt

      	.sect   ".vect"

_int00      br _c_int00         
_tint1    	br _c_int03    	;TINT1 external interrupt from FPAG		
   		
         .end