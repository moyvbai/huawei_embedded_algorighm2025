g++ main.cpp -O2  -o main.exe && main.exe < ../benchmark3/in/1.in > ../tmp/in11.txt
g++ -std=c++17 -O2 -o validator.exe validator.cpp
validator.exe ../benchmark3/in/1.in  ../tmp/in11.txt