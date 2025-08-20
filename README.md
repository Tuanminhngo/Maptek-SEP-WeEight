# Maptek-SEP-BLOCK15
Software Engineering &amp; Project - group BLOCK-15 Maptek

Do not care about the debug files, they are quite messy. I use it only for debugging. 

### How to run 

For Mac:
- Compile python file:
    ```bash
    cat input.txt | python3 stream_processor.py
    ```
- Compile c++ file:
    ```bash
    cat input.txt | ./stream_processor > output.txt
    ```
- Create .exe file:
    ```bash
    x86_64-w64-mingw32-g++ stream_processor.cpp -o stream_processor.exe -static -std=c++17
    ```

For Windows:
- Compile python file:
    ```bash
    python stream_processor.py < input.txt > output.txt
    ```
- Compile c++ file:
    ```bash
    type input.txt | stream_processor.exe > output.txt
    ```
- Create .exe file:
    ```bash
    g++ stream_processor.cpp -o stream_processor.exe -static -std=c++17
    ```