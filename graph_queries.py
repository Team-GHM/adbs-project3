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
plt.figure()
plt.plot(nops, throughputs, label='Throughput')
plt.xlabel('nops')
plt.ylabel('Time taken to perform one operation')
plt.title('Time taken to perform nops')
plt.legend()
plt.grid()
plt.savefig('/home/u1418793/adbs-project3/queries.png') 
plt.show()