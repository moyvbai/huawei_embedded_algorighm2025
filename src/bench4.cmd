g++ main.cpp -o main.exe && main.exe < ../benchmark1/in/4.in > ../tmp/in4.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../benchmark1/in/4.in  ../tmp/in4.txt