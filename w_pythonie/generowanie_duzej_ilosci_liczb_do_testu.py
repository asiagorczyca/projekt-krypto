import os
k="python3 proba_generowania_liczb_losowych_bezpiecznie.py --bytes 1024"
with open("output.txt", "w", encoding="utf-8") as plik:
    for i in range(100000):
        #plik.write(f"\n### {k} ###\n")
        if (i+1)%20==0: print((i+1),".proba")
        wynik = os.popen(k).read()
        plik.write(wynik)