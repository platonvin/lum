basic types wrapped to check for overflow, zero division, NAN's, nullptr deref, etc. WHen such error found it is reported, as well as file + line + function where it occured. Note: currently, it only tracks location where constructor for SafeType called (so a += SafeInt<int>(1) would print that exact line, but a += b would print lines where a and b declared)
they all compile to same asm when checks are disabled
suggest improvements via issues



