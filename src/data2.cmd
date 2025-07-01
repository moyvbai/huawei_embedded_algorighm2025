g++ main.cpp -o main.exe && main.exe < ../data/1npu_k2_m1000_2.in > out2.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../data/1npu_k2_m1000_2.in  out2.txt