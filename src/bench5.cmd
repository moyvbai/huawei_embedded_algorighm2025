g++ main.cpp -O2  -o main.exe && main.exe < ../benchmark1/in/5.in > ../tmp/in5.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../benchmark1/in/5.in  ../tmp/in5.txt