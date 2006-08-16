#!/usr/bin/env python
# csvexport.py
# Rewritten by: geoscope
# This file reads units.csv and prints out a more easily human readable entry for units.
# Note, there is a primitive pattern matching ability.
#	csvexport.py units.csv .blank		would print all .blank variant ships.
# Program should work standalone, or as a module.
# Other programs using csvexport.py would need updating to use:
#	csvexport.CsvExport
# Files generated: now use the key field for file name generation, not the command line arguement.
#_____________________________________________________________________________________________________
USAGE = """USAGE: csvexport.py <filename> OPTIONS <unitname(s)>
     <filename> =		<path/>units.csv
        OPTIONS = -units	to display units of measurment in the output, default does not
                  -toscreen	writes output to screen, default prints to "<unitname>.csv"
     <unitname> = llama goddard	to print information on llama and goddard"""
#_____________________________________________________________________________________________________
import sys, os, csv # the Vegastrike csv.py module, not the standard python csv module.
# It should be in the same directory as csvexport.py and csvimport.py

def CsvExport():
    global showunits, ToScreen
    showunits=0
    ToScreen=0
    if len(sys.argv) < 3:
	print
	print sys.argv[0], USAGE
	return
    if (sys.argv[1].find('.csv') != -1): # find returns -1 if string NOT found, -1 == bool True
        path=sys.argv[1]
        args=sys.argv[2:]
    else:
        print 
	print sys.argv[0], USAGE
        return

    for i in args:	# easy to add OPTIONS, if you do, please add to USAGE information as well
        if (i=="-units"):
	    showunits=1
	if (i=="-toscreen"):
	    ToScreen=1
    path ="units.csv"
    workfile = file(path, "r")
    KeyList = csv.semiColonSeparatedList(workfile.readline().strip())
    GuideList = csv.semiColonSeparatedList(workfile.readline().strip())
    Lines = workfile.readlines()
    workfile.close()
    
    KeyLL = len(KeyList)
    GuideLL = len(GuideList)
    numLine = 0
    for line in Lines:
	numLine += 1
	for arg in args:
	    if (line.find(arg) != -1):
                #if (csv.earlyStrCmp(line,arg)): # I'm not usre what this did, but it didn't seem necesary
                line = line.strip()
                row = csv.semiColonSeparatedList(line)
                rowLL = len(row)
	        Entry = row[0]
	        #if (rowLL!=KeyLL or rowLL!=GuideLL):
	        #    print "Mismatch in row length", rowLL, "for", row[0], "with key len =", KeyLL, "and guide len =", GuideLL
	        #    print "Contains only", rowLL, "fields"
	        #    break
	        # The above block should work, rarely, to catch errors... and next line should be elif rowLL:
	        # It fails miserably because most lines in units.csv do not have the same number of fields.
                #if rowLL:	# bool False, if row list length is zero... an empty line in file, this suppresses blank line results.
                print "Line", numLine, ": Columns ", rowLL, "Entry:", Entry
	    
		if ToScreen:
		    for i in range(rowLL):
	                if GuideList[i].find('{')!=-1:
	                    print csv.writeList([KeyList[i]]+ProcessList(GuideList[i],row[i]))
	                elif GuideList[i].find(';')!=-1:
	                    print csv.writeList([KeyList[i]]+ProcessStruct(GuideList[i],row[i]))
	                else:
	                    print csv.writeList([makeName(KeyList[i],GuideList[i]), row[i]])
                else:
		    outfile = file(Entry+".csv", "w")
		    for i in range(rowLL):
	                if GuideList[i].find('{')!=-1:
	                    outfile.write(csv.writeList([KeyList[i]]+ProcessList(GuideList[i],row[i])))
	                elif GuideList[i].find(';')!=-1:
	                    outfile.write(csv.writeList([KeyList[i]]+ProcessStruct(GuideList[i],row[i])))
	                else:
	                    outfile.write(csv.writeList([makeName(KeyList[i],GuideList[i]), row[i]]))


    print
    print "Number of Keys:", KeyLL, "\tNumber of Guides:", GuideLL 
    print """By strickest definition of CSV, Every record should have the same number
    of Columns (commas) as above. units.csv does not. The old csvexport.py would have
    failed most of these records."""

def filterParen(l):
	global showunits
	if (showunits):
		return l
	where=l.find("(");
	if (where!=-1):
		return l[0:where]
	return l;

def interleave (l1,l2,add1,add2):
	ret=[]
	for i in range(len(l1)):
		ret.append(add1+filterParen(l1[i])+add2);
		ret.append(l2[i]);
	return ret

def makeName(nam,guide):
	global showunits
	if (showunits):
		return nam+'('+guide+')'
	return nam

def ProcessStruct (guide,struc):
	if (len(struc)==0):
		for i in range(guide.count(";")):
			struc+=";"
	l=struc.split(';')
	g=guide.split(';')	
	return interleave(g,l," ",'=');

def ProcessList(guide,row):
	og=guide.find('{');
	cg=guide.find('}');
	if (og==-1 or cg== -1):
		print "error in "+str(row)+" "+str(guide)
		return ""
	guide=guide[og+1:cg]
	ret=[]
	while(1):
		_or=row.find('{');
		_cr=row.find('}');
		if (_or==-1 or _cr==-1):
			break;
		ret+=['{']
		ret+=ProcessStruct(guide,row[_or+1:_cr])		
		row=row[_cr+1:]
		ret+=['}']
	ret+=['{']
	ret+=ProcessStruct(guide,"")
	ret+=['}']
	return ret

if __name__ == "__main__":
    CsvExport()
    
