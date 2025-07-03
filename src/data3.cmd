g++ main.cpp -o main.exe && main.exe < ../data/10npu_1.in > ../tmp/out3.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../data/10npu_1.in  ../tmp/out3.txt