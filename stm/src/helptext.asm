/**
 * @file    helptext.asm
 * @author  Peter Feichtinger
 * @date    25.05.2014
 * @brief   Includes the command line help text from command-line.txt file.
 */
 
    .section .data.helptext_start
    .global helptext_start
helptext_start:
    .incbin "../command-line.txt"
    
    .global helptext_end
helptext_end:
    .byte 0
    
    .global helptext_size
helptext_size:
    .int helptext_end - helptext_start
