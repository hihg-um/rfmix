#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include "rfmix.h"
#include "kmacros.h"
#include "s-sample.h"

extern rfmix_opts_t rfmix_opts;

static void permute(int *a, int n) {
  for(int i=0; i < n; i++) {
    int j = rand()/(RAND_MAX + 1.0) * n;
    int tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
  }
}

static void generate_refsample_map(int **ref_map, int *n_ref, sample_t *samples, int n_samples,
				   int n_subpops) {
  for(int i=0; i < n_subpops; i++) n_ref[i] = 0;

  for(int i=0; i < n_samples; i++) {
    if (samples[i].apriori_subpop != -1)
      n_ref[samples[i].apriori_subpop]++;
  }

  for(int k=0; k < n_subpops; k++) {
    if (n_ref[k] > 0) {
      ref_map[k] = new int[n_ref[k]];
    } else {
      ref_map[k] = NULL;
    }
    n_ref[k] = 0;
  }
    
  for(int i=0; i < n_samples; i++) {
    int k = samples[i].apriori_subpop;
    if (k == -1) continue;

    ref_map[k][n_ref[k]++] = i;
  }
}

static int select_parents(S_Sample ***r_ssamples, sample_t *samples, int n_samples,
			  int n_subpops, snp_t *snps, int n_snps) {
  int *ref_map[n_subpops];
  int n_ref[n_subpops];
    
  generate_refsample_map(ref_map, n_ref, samples, n_samples, n_subpops);
  for(int k=0; k < n_subpops; k++)
    permute(ref_map[k], n_ref[k]);
  
  int s_ref[n_subpops];
  int n_parents = 0;
  for(int k=0; k < n_subpops; k++) {
    s_ref[k] = n_ref[k]*SIM_PARENT_PROPORTION + 1;
    n_parents += s_ref[k];
  }

  S_Sample **ssamples = new S_Sample*[n_parents];
  n_parents = 0;
  for(int k=0; k < n_subpops; k++) {
    for(int i=0; i < s_ref[k]; i++) {
      char id[10];
      sprintf(id,"%08x", (uint32_t)rand() + (uint32_t)rand());
      ssamples[n_parents] = new S_Sample(id, k, snps, n_snps,
					samples[ ref_map[k][i] ].haplotype[0],
					samples[ ref_map[k][i] ].haplotype[1]);
      samples[ ref_map[k][i] ].s_parent = 1;
      n_parents++;
    }
  }

  for(int k=0; k < n_subpops; k++)
    delete[] ref_map[k];
    
  *r_ssamples = ssamples;
  return n_parents;  
}

void generate_simulated_samples(input_t *input) {
  S_Sample **parents;
  S_Sample **children;
  int n_parents = select_parents(&parents, input->samples, input->n_samples, input->n_subpops,
				 input->snps, input->n_snps);

  int n_start_parents = n_parents;
  double growth_rate = SIM_GROWTH_RATE;
  int max_samples = input->n_subpops * SIM_SAMPLES_PER_SUBPOP;
  for(int g=0; g < rfmix_opts.n_generations; g++) {
    for(int i=0; i < n_parents; i++) {
      int j = rand()/(RAND_MAX + 1.0) * n_parents;
      S_Sample *tmp = parents[i];
      parents[i] = parents[j];
      parents[j] = tmp;
    }

    int next_size = n_parents * growth_rate;
    if (next_size > max_samples) next_size = max_samples;
    children = new S_Sample*[next_size];
    for(int i=0; i < next_size; i++) {
      children[i] = new S_Sample(parents[i % n_parents], parents[(i+1) % n_parents]);
    }

    for(int i=0; i < n_parents; i++)
      delete parents[i];
    delete[] parents;

    parents = children;
    n_parents = next_size;
  }
  fprintf(stderr,"\nInternally simulated %d samples from %d randomly selected reference parents.\n",
	  n_parents, n_start_parents);
  
  RA(input->samples, sizeof(sample_t)*(input->n_samples + n_parents), sample_t);
  int n_snps = input->n_snps;
  int n_windows = input->n_windows;
  int t = input->n_samples;
  int n_subpops = input->n_subpops;
  sample_t *samples = input->samples;
  for(int i=0; i < n_parents; i++,t++) {
    samples[t].s_parent = 0;
    samples[t].s_sample = 1;
    samples[t].sample_id = strdup(parents[i]->sample_id);
    samples[t].apriori_subpop = -1;
    samples[t].column_idx = -1;
    samples[t].sample_idx = i;
    for(int j=0; j < 4; j++) {
      MA(samples[t].est_p[j], sizeof(int16_t)*n_windows*n_subpops, int16_t);
      for(int l=0; l < n_windows; l++) {
	for(int k=0; k < n_subpops; k++)
	  samples[t].est_p[j][IDX(l,k)] = ef16(0.01/(n_subpops-1));
      }
      MA(samples[t].msp[j], sizeof(int8_t)*n_windows, int8_t);
    }
    for(int j=0; j < 2; j++) {
      MA(samples[t].haplotype[j], sizeof(int8_t)*n_snps, int8_t);
      memcpy(samples[t].haplotype[j], parents[i]->haplotype[j], sizeof(int8_t)*n_snps);
      MA(samples[t].ksp[j], sizeof(int8_t)*n_windows, int8_t);
      for(int l=0; l < n_windows; l++) {
	samples[t].ksp[j][l] = parents[i]->subpop[j][ input->crf_windows[l].snp_idx ];

	double p[n_subpops];
	for(int k=0; k < n_subpops; k++) p[k] = 0.1;
	for(int s=input->crf_windows[l].rf_start_idx; s < input->crf_windows[l].rf_end_idx; s++)
	  p[parents[i]->subpop[j][s]]++;
	double d = 0.;
	for(int k=0; k < n_subpops; k++) d += p[k];
	for(int k=0; k < n_subpops; k++) 
	  samples[t].est_p[j][IDX(l,k)] = ef16(p[k]/d);
      }
	
      MA(samples[t].current_p[j], sizeof(int16_t)*n_windows*input->n_subpops, int16_t);
      MA(samples[t].sis_p[j], sizeof(float)*n_windows, float);
    }
  }
  input->n_samples += n_parents;
  
}

void print_simulation_scoring_matrix(double **m, int n) {

  for(int j=0; j < n; j++)
    fprintf(stderr,"\t%d",j);
  fprintf(stderr,"\n");
  
  for(int i=0; i < n; i++) {
    fprintf(stderr,"%d",i);
    for(int j=0; j < n; j++) {
      fprintf(stderr,"\t%1.1f", m[i][j]*100.);
    }
    fprintf(stderr,"\n");
  }   
}

void free_simulation_scoring_matrix(double **m, int n) {
  for(int i=0; i < n; i++)
    delete[] m[i];
  delete[] m;
}

static double det(double **sm, int n) {

  double **m = new double*[n];
  for(int i=0; i < n; i++) {
    m[i] = new double[n];
    for(int j=0; j < n; j++) m[i][j] = sm[i][j];
  }
  
  for(int j=0; j < n; j++) {
    for(int i=j+1; i < n; i++) {
      double f = m[i][j]/m[j][j];
      if (f < 1e-7) continue;
      
      for(int k=0; k < n; k++)
	m[i][k] -= m[j][k]*f;
    }
  }

  double d = 1.;
  for(int i=0; i < n; i++)
    d *= m[i][i];

  free_simulation_scoring_matrix(m, n);
  return d;
}

double score_msp(double ***r_m, double *r_ha, input_t *input) {

  int n_subpops = input->n_subpops;
  double **m = new double*[n_subpops];
  for(int k=0; k < n_subpops; k++) {
    m[k] = new double[n_subpops];
    for(int l=0; l < n_subpops; l++) {
      m[k][l] = 0.;
    }
  }
  
  for(int i=0; i < input->n_samples; i++) {
    sample_t *sample = input->samples + i;
    if (sample->s_sample == 0) continue;

    for(int h=0; h < 2; h++) {
      for(int j=0; j < input->n_windows; j++)
	m[sample->msp[h][j]][sample->ksp[h][j]]++;
      /*	for(int k=0; k < input->n_subpops; k++) {
	  m[k][sample->ksp[h][j]] += DF16(input->samples[i].current_p[h][IDX(j,k)]);
	  }*/
    }
  }

  for(int k=0; k < n_subpops; k++) {
    double d = 0.;
    for(int l=0; l < n_subpops; l++) {
      d += m[l][k];
    }
    for(int l=0; l < n_subpops; l++) {
      m[l][k] /= d;
    }
  }

  double ha = 0.;
  for(int k=0; k < n_subpops; k++) {
    ha += m[k][k];
  }
  ha /= n_subpops;
  *r_ha = ha;
  
  *r_m = m;
  return det(m, n_subpops);
}

