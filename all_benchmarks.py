import os

f = open('all_benchmarks_to_run', 'w')

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
                f.write(f'{c}/{x}\n')
f.close()