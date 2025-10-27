from math import gcd
i=0
wzglednie_pierwsze=0
wszystkie=0
with open("output.txt","r") as dane:
    for wiersz in dane:
        i+=1
        wiersz=wiersz.strip()
        if i%2==1: liczba1=int(wiersz,16)
        else: 
            liczba2=int(wiersz,16)
            wszystkie+=1
            wzglednie_pierwsze+=(gcd(liczba1,liczba2)==1)
nasze_p=6*(wszystkie/wzglednie_pierwsze)
pi=nasze_p**(0.5)
print(pi)

