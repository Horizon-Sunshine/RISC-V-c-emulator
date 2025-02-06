input files are trace1.txt and trace2.txt, the program always reads trace1.txt so in order to feed a different .txt file you need to change its name to trace1.txt.
the output will be in output.txt.
Command line arguments:
forwarding: 0 or 1 for supported (all types) or unsupported.
early branch resolution: 0 or 1, 1 for branch resolution in ID(Decode) part of the pipeline, 0 for normal resolution.
How to run:
1. compile riscv.c using your favourite c compiler.
2. run the exe from the cmd using the appropriate command line arguments.
Example: riscv 1 1
the above example activates the program WITH support for both forwarding and early branch resolution.
