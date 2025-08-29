# Maptek-SEP-BLOCK15
Software Engineering &amp; Project - group BLOCK-15 Maptek




# compressor

Quickly went through the header file:

- **App.hpp** is the high-level run loop and configuration, it hold the global parameter (x,y,z) for the whole program, and coordinate other module (IO, Worker,...) to ensure they work together.

- **IO.hpp** is the input/output of the compressor, it continuesly check and read the parent block, then hand it back to App.hpp for processing, later on, it write the result to the output stream.
    
- **Model.hpp** define the data structure that will be used accross the program, these contain:

    - LabelTable: the dictionary to translate the block ***(do we need this?)*** ***(Nah we gonna need it)***
    - BlockDes: the compressed presentation of block(s) through ids
    - Grid: a 3D container (matrix) of label id present the raw content of a parent block.
    - ParentBlock: the combination of it own **Grid** and external information(its position on the whole map)

- **Strategy.hpp** contain different algorithm we gonna use for the compress/grouping

- **Worker.hpp** is the middle part between **Coordinator** and **Strategy**, it receives a *ParentBlock* from I/O, coordinates one or more strategies to compress it, and produces a list of *BlockDesc objects* ready for output.


# Go to `compressor/docs` for UML