g++ main.cpp -o main.exe && main.exe < ../data/data.in > ../tmp/out.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../data/data.in  ../tmp/out.txt