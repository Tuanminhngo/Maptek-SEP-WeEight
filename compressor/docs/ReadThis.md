## UML

First, the program contain 5 main component:

    - Coordinator
    - IO
    - Model
    - Strategy
    - Worker

The model provide 4 core data strucure for the whole rogram, incude **LabelTable**, **Grid**, **ParentBlock**, **BlockDesc** *(read the [README.md](../../README.md) for more info)*
**Grid** is a component of **ParentBlock**

**Worker** and **Strategy** all have interface for future development

**Coordinator** orchestrate everything, *Endpoint*(in **IO**) manages I/O and own *LabelsTable*, **Worker** compress block through *one or many* **Strategy**, and *DefaultStrat* is a sample algorithm plugged into that **Strategy** interface.

## Sequence

The program start by calling `Coordinator::run()`, which initializes the *Endpoint*. The **Coordinator** then enters a loop: 

While the *Endpoint* has more parent blocks, it retrieves a *ParentBlock* and sends it to the **Worker**. The **Worker** calls its **Strategy** (e.g. *DefaultStrat*) for each label present in the block, producing a set of *BlockDesc* objects. These results are returned to the **x**, which instructs the Endpoint to write them out. The process repeats until no more parent blocks remain, at which point the program finishes.