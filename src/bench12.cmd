g++ main.cpp -O2  -o main.exe && main.exe < ../benchmark3/in/2.in > ../tmp/in12.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../benchmark3/in/2.in  ../tmp/in12.txt