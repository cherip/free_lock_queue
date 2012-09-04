#! /bin/python

pf = open('log')
unique = set()
for line in pf:
    if line.find('-1') == -1:
        if line not in unique:
            unique.add(line)
        else:
            print 'Error'
