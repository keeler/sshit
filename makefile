sshit: sshit.o sshitd.o
	g++ -O3 -g -Wall -o sshit sshit.o
	g++ -O3 -g -Wall -o sshitd sshitd.o

sshit.o: sshit.cpp
	g++ -O3 -g -Wall -c sshit.cpp

sshitd.o: sshitd.cpp
	g++ -O3 -g -Wall -c sshitd.cpp

clean:
	rm -f sshit sshitd *.o

