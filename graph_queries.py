import matplotlib.pyplot as plt

# Function to load data from a file
def load_data(file_path):
    nops = []
    ops_times = []

    with open(file_path, 'r') as file:
        lines = file.readlines()

    for line in lines:
        values = line.split()
        nops.append(int(values[0]))
        ops_times.append(float(values[1]))

    return nops, ops_times

# Load data for reads
nops_read_new, ops_times_read_new = load_data('read_ops_times_new.txt')
nops_read_old, ops_times_read_old = load_data('read_ops_times_old.txt')

# Load data for writes
nops_write_new, ops_times_write_new = load_data('write_ops_times_new.txt')
nops_write_old, ops_times_write_old = load_data('write_ops_times_old.txt')

split_index = 700

# Create subplots for reads

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

# Plot for Reads
ax1.plot(nops_read_new[:split_index], ops_times_read_new[:split_index], color='red', label='Adaptable Reads')
ax1.plot(nops_read_new[split_index:], ops_times_read_new[split_index:], color='purple', label='Adaptable Writes')
ax1.plot(nops_read_old[:split_index], ops_times_read_old[:split_index], color='blue', label='NonAdaptable Reads') 
ax1.plot(nops_read_old[split_index:], ops_times_read_old[split_index:], color='cyan', label='NonAdaptable Writes')

ax1.legend()
ax1.set_xlabel('Operation(hundreds)', fontsize=18)
ax1.set_ylabel('Average time (μs)', fontsize=18)
ax1.set_title('Read Heavy Operations (70% Reads and 30% Writes)',fontsize=16)
#ax1.set_title('Read Heavy Operations',fontsize=16)

# Plot for Writes
ax2.plot(nops_write_new[:split_index], ops_times_write_new[:split_index], color='green', label='Adaptable Writes')
ax2.plot(nops_write_new[split_index:], ops_times_write_new[split_index:], color='olive', label='Adaptable Reads')  
ax2.plot(nops_write_old[:split_index], ops_times_write_old[:split_index], color='orange', label='NonAdaptable Writes')
ax2.plot(nops_write_old[split_index:], ops_times_write_old[split_index:], color='brown', label='NonAdaptable Reads')
ax2.legend()
ax2.set_xlabel('Operation(hundreds)',fontsize=18)
ax2.set_ylabel('Average time (μs)',fontsize=18)
ax2.set_title('Write Heavy Operations (70% Writes and 30% Reads)',fontsize=16)


# Adjust layout and save the figure
plt.tight_layout()
plt.savefig('/home/u1418793/adbs-project3/operations_plots.png')

# Show the plots
plt.show()
