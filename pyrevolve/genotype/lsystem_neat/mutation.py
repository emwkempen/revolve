from ..plasticoding.mutation.standard_mutation import standard_mutation as plasticondig_mutation
from ..neat_brain_genome.mutation import mutation_complexify as neat_mutation

from ..plasticoding.mutation.mutation import MutationConfig as PlasticodingMutationConf
from ..neat_brain_genome import NeatBrainGenomeConfig


class LSystemNeatCrossoverConf:
    def __init__(self,
                 plasticoding_crossover_conf: PlasticodingMutationConf,
                 neat_crossover_conf: NeatBrainGenomeConfig):
        self.plasticoding = plasticoding_crossover_conf
        self.neat = neat_crossover_conf


def composite_mutation(genotype, body_mutation, brain_mutation):
    new_genome = genotype.clone()
    new_genome._body_genome = body_mutation(new_genome._body_genome)
    new_genome._brain_genome = brain_mutation(new_genome._brain_genome)
    return new_genome


def standard_mutation(genotype, mutation_conf: LSystemNeatCrossoverConf):
    return composite_mutation(
        genotype,
        lambda g: plasticondig_mutation(g, mutation_conf.plasticoding),
        lambda g: neat_mutation(g, mutation_conf.neat),
    )
