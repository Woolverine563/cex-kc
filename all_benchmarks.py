import os

f = open('all_benchmarks_to_run', 'w')

d = {}

for a in os.listdir('benchmarks'):
    if os.path.isdir(f'benchmarks/{a}'):
        b = f'benchmarks/{a}'
        c = ''
        if (os.path.exists(f'{b}/verilog')):
            c = f'{b}/verilog'
        else:
            c = f'{b}/aiger'
        for x in os.listdir(c):
            if os.path.isfile(f'{c}/{x}'):
                d[x] = f'{c}/{x}'

for v in d.values():
    f.write(f'{v}\n')
f.close()