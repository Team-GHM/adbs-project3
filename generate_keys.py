import random
from scipy.stats import skewnorm
import numpy as np
import matplotlib.pyplot as plt

def generate_keys_and_plot(number_of_distinct_keys, nops, skewness):
    # Generate random keys
    random_keys = [random.randint(0, number_of_distinct_keys - 1) for _ in range(nops)]
    save_keys_to_file(random_keys, 'random_keys.txt')

    # Generate skewed keys
    mean_value = number_of_distinct_keys / 2.0
    scale_value = number_of_distinct_keys / 6.0
    skewnorm_dist = skewnorm(loc=mean_value, scale=scale_value, a=skewness)
    skewed_keys = skewnorm_dist.ppf(np.linspace(0, 1, nops))
    skewed_keys = np.round(skewed_keys).astype(int) % number_of_distinct_keys
    save_keys_to_file(skewed_keys, 'skewed_keys.txt')

    # Plotting both histograms in the same plot
    plt.hist(random_keys, bins=50, density=True, alpha=0.5, label='Random Keys', edgecolor='black')
    plt.hist(skewed_keys, bins=50, density=True, alpha=0.5, label='Skewed Keys', color='b', edgecolor='black')

    plt.title('Keys Distribution')
    plt.xlabel('Keys')
    plt.ylabel('Probability Density')
    plt.legend()  # Show legend for labels
    plt.show()

    # Save the combined histogram plot to a file
    plt.savefig('/home/u1418793/adbs-project3/combined_keys.png')

def save_keys_to_file(keys, filename):
    with open(filename, 'w') as file:
        for key in keys:
            value = str(key) + ":"
            file.write(f"{key} {value}\n")

if __name__ == "__main__":
    number_of_distinct_keys = 100000  # Set to the desired number of distinct keys
    nops = 100000  # Set to the desired number of operations
    skewness = 2.0  # Adjust the skewness parameter as needed

    generate_keys_and_plot(number_of_distinct_keys, nops, skewness)

