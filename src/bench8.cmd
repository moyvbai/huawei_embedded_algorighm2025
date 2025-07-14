g++ main.cpp -O2 -o main.exe && main.exe < ../benchmark1/in/8.in > ../tmp/in8.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../benchmark1/in/8.in  ../tmp/in8.txt