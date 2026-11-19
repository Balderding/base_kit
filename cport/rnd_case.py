"""The random-number proof, Python side: prints one line per result for a fixed script of calls on many seeds.
cport/bin/rndtest.exe prints the same script from the C port; the two outputs must be identical.
    python cport/rnd_case.py > a.txt ; cport\\bin\\rndtest.exe > b.txt ; fc a.txt b.txt"""
import random, struct, itertools

def h(x): return struct.pack("<d", x).hex()

SEEDS = [("s", ""), ("s", "a"), ("s", "badlands12tier"), ("s", "mine3mix"), ("s", "ground_bigbadlandsbadlands_12proof1"), ("s", "ü-ëΩ"), ("s", "x" * 200),
         ("i", 0), ("i", 1), ("i", 12345), ("i", 2 ** 40 + 7), ("i", 2 ** 63 - 1),
         ("f", 0.0), ("f", 0.5), ("f", 0.123456789), ("f", 0.9999999999999999), ("f", 1e-300), ("f", 123.456), ("f", 2.0 ** 60), ("f", 7.0)]

def script(r, out):
    out.append(h(r.random())); out.append(h(r.random())); out.append(h(r.uniform(-2.5, 7.0))); out.append(h(r.uniform(3, 1)))
    out.append(str(r.randint(1, 6))); out.append(str(r.randint(0, 10 ** 9))); out.append(str(r.randint(-5, 5)))
    out.append(str(r.randrange(0, 100, 7))); out.append(str(r.randrange(10, 0, -3))); out.append(str(r.randrange(3, 40)))
    out.append(str(r.choice(range(17))))
    x = list(range(12)); r.shuffle(x); out.append(",".join(map(str, x)))
    out.append(",".join(map(str, r.sample(range(30), 5)))); out.append(",".join(map(str, r.sample(range(1000), 20)))); out.append(",".join(map(str, r.sample(range(8), 8))))
    out.append(",".join(map(str, r.sample(range(12), 6))))
    w = [1, 2, 3.5, 0.25]; out.append(",".join(str(i) for i in r.choices(range(4), w, k=5)))
    out.append(",".join(str(i) for i in r.choices(range(9), k=4)))
    out.append(str(r.getrandbits(40))); out.append(str(r.getrandbits(64))); out.append(str(r.getrandbits(1)))
    for _ in range(700): r.random()          # past one twist of the generator
    out.append(h(r.random()))

if __name__ == "__main__":
    for kind, v in SEEDS:
        r = random.Random(v); out = []; script(r, out)
        print(f"{kind} {v!r:.40}: " + " ".join(out))
