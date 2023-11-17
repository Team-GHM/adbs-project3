import random
from scipy.stats import zipf
from scipy.stats import skewnorm
import numpy as np
import matplotlib.pyplot as plt

def generate_random_keys(number_of_distinct_keys, nops):
    keys = []
    with open("random_keys.txt", "w") as file:
        for _ in range(nops):
            key = random.randint(0, number_of_distinct_keys - 1)
            keys.append(key)
            value = str(key) + ":"
            file.write(f"{key} {value}\n")

    """plt.hist(keys, bins=50, density=True, alpha=0.5, edgecolor='black')
    plt.title('Uniform Random Keys')
    plt.xlabel('Keys')
    plt.ylabel('Probability Density')
    plt.savefig('/home/u1418793/adbs-project3/r_keys.png') 
    plt.show()"""

def generate_skewed_keys(number_of_distinct_keys, nops, skewness):
    mean_value = number_of_distinct_keys / 7.0  
    scale_value = number_of_distinct_keys / 6.0  # Set the scale (standard deviation) value
    
    skewnorm_dist = skewnorm(loc=mean_value, scale=scale_value, a=skewness)

    # Generate keys based on the inverse cumulative distribution function (ppf)
    keys = skewnorm_dist.ppf(np.linspace(0, 1, nops))
    keys = np.round(keys).astype(int) % number_of_distinct_keys

    with open("skewed_keys.txt", "w") as file:
        for key in keys:
            value = str(key) + ":"
            file.write(f"{key} {value}\n")

    # Plotting the distribution
    plt.hist(keys, bins=50, density=True, alpha=0.5, color='b', edgecolor='black')
    plt.title('Skewed Keys Distribution')
    plt.xlabel('Keys')
    plt.ylabel('Probability Density')
    plt.savefig('/home/u1418793/adbs-project3/s_keys.png') 
    plt.show()


if __name__ == "__main__":
    number_of_distinct_keys = 2 ** 10  # Set to the desired number of distinct keys
    nops = 2 ** 12  # Set to the desired number of operations
    skewness = 2.0  # Adjust the skewness parameter as needed

    generate_random_keys(number_of_distinct_keys, nops)
    generate_skewed_keys(number_of_distinct_keys, nops, skewness)
