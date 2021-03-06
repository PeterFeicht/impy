/**
 * @file    helptext.asm
 * @author  Peter Feichtinger
 * @date    25.05.2014
 * @brief   Includes the command line help text from command-line.txt file.
 * 
 * Note that this file needs to be manually rebuilt when command-line.txt changes since the Eclipse build system
 * doesn't notice that.
 */
 
    .section .text.helptext
    .global helptext_start
helptext_start:
    .incbin "../command-line.txt"
    
    .global helptext_end
helptext_end:
    .byte 0
