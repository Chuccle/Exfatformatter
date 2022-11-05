# Exfatformatter
A better and more efficient way of formatting a drive into exfat than sdFat.

ExFatFormatter borrows heavily from [SdFat](https://github.com/greiman/SdFat/blob/master/src/ExFatLib/ExFatFormatter.cpp). 
Specifically it's implementation of Exfat formatting.

<br>

Upon understanding the limitations of sdfat, I decided to allow it to be much more dynamic, allow more efficient cluster sizing 
and follow Microsoft official documentation. 

<br>

You will notice instead of calculating the upcase table, It's premade inside a header. This upcase table follows the Microsoft specification.
Another removed limitation is the minimum volume size of 500MB. this was to do with the previous algorithm not supporting multiple clusters to 
make up the allocation bitmap and upcase table etc.
