import matplotlib.pyplot as plt

# Load the data from the output file
with open('throughput.txt', 'r') as file:
    lines = file.readlines()

nops = []
throughputs = []

for line in lines:
    values = line.split()
    nops.append(int(values[0]))
    throughputs.append(float(values[1]))

# Create the time series graph
plt.figure(figsize=(10,8))
plt.plot(nops, throughputs, label='Time taken by operations')
plt.xlabel('nops')
plt.ylabel('Time taken to for one operation (μs)')
plt.title('Time taken to perform nops')
plt.legend()
plt.grid()

#y_tick_positions = range(int(min(throughputs)), int(max(throughputs) + 1), 10000)  # Custom interval of 10000
#plt.yticks(y_tick_positions, [str(val) for val in y_tick_positions])

plt.savefig('/home/u1418793/adbs-project3/queries.png') 
plt.show()