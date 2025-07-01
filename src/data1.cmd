g++ main.cpp -o main.exe && main.exe < ../data/1npu_k2_m1500.in > out1.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../data/1npu_k2_m1500.in  out1.txt