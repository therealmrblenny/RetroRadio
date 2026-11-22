import json, sys
d = json.load(open('out/frames.json'))
run = d['runs'][int(sys.argv[1])]
ms = d['frameMs']
for t in map(int, sys.argv[2:]):
    h = run['frames'][t // ms]
    cols = [int(h[i*2:i*2+2], 16) for i in range(len(h)//2)]
    print(f'--- t={t} b={run["bright"][t//ms]}')
    for y in range(8):
        print(''.join('#' if cols[x] >> y & 1 else '.' for x in range(len(h)//2)))
