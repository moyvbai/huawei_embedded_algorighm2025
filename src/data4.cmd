g++ main.cpp -o main.exe && main.exe < ../data/hand1.in > ../tmp/out4.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../data/hand1.in  ../tmp/out4.txt