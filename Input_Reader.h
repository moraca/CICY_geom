//===========================================================================
//SOFTWARE:     3D geometric model for CNT and GS networks
//OBJECTIVE:    Read input data and save it into data structures
//AUTHOR:       Angel Mora
//E-MAIL:       angel.mora@cicy.mx
//===========================================================================

#ifndef INPUTREADER_H
#define INPUTREADER_H

#include<iomanip>
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<string>
#include "time.h"
#include "Hns.h"
using namespace hns;

#include "Geometry_3D.h"

const double PI = 3.1415926535897932384626433832795;
const int MAX_ATTEMPTS = 15;

//---------------------------------------------------------------------------
//Name of application case
struct App_name{
    string keywords;
    bool mark;
    string str;
};
//Name of simulation
struct Simu_para{
    string keywords;
    bool mark;
    string simu_name;
    string create_read_network;
    int sample_num;
    int avoid_resistance;
    string particle_type;
    //Seeds (positive integers) for the random number generators to generate a network
    vector<unsigned int> network_seeds;
};
//The geometry of the sample
struct Geom_sample{
    string keywords;
    bool mark;
    Point_3D origin;												//Define an origin point for a sample
    double len_x, wid_y, hei_z;								//Define length, width and height for an extended RVE for generation with an acurrate control
    Point_3D ex_origin;											//Define an origin point for an extended sample to generate network with an acurrate control
    double ex_len, ey_wid, ez_hei;							//Define length, width and height for an extended sample for generation with an acurrate control
    double volume;
    double matrix_density;            //Define the density of matrix
    double gs_minx, gs_miny, gs_minz;					//Define the minimum size for background grids (looking for contact points)
    double win_max_x, win_max_y, win_max_z;		//Define the size range of the cutoff window and descrement by every step in x, y and z directions
    double win_min_x, win_min_y, win_min_z;
    double win_delt_x, win_delt_y, win_delt_z;
    int cut_num;														//Define the number of cutoff times (0: the maxmum size, n: the maxmum size - n*step_length(delta), n>=1)
    
};
//The nanotube parameters in a network
struct Nanotube_Geo{
    string keywords;
    bool mark;
    string criterion;					//Define the volume or weight fraction of nanotubes in the sample: vol, wt
    string dir_distrib_type;			//Define the initial growth direction type (random or specific) in a RVE
    string len_distrib_type;			//Define the distribution type (uniform or normal) of the length (unit: micrometer) of nanotubes
    string rad_distrib_type;			//Define the distribution type (uniform or normal) of the radius (unit: micrometer) of nanotubes
    double step_length;				//Define the step length (unit: micromether) of nanotube growth
    double ini_sita, ini_pha;			//Define initial direction for 'specific' type in the spherical coordinates
    double angle_max;				//Define the angle 'omega' for the normal distribution range [-omega, omega] of the growth direction
    double len_min, len_max;		//Define the length range (min, max) of nanotubes
    double rad_min, rad_max;		//Define the radius range (min,max) of nanotubes
    double volume_fraction;		//Define the volume fraction of nanotubes
    double real_volume;				//Define the real volume of nanotubes
    double weight_fraction;			//Define the weight fraction of nanotubes
    double real_weight;				//Define the real weight of nanotubes
    double linear_density;			//Define the linear density of nanotubes
    double density;			//Define the density of nanotubes
};
//The nanotube parameters in a network
struct GNP_Geo{
    string keywords;
    bool mark;
    string criterion;					//Define the volume or weight fraction of GNPs in the sample: vol, wt
    string growth_type;                 //Define if the generation of the CNTs on the GNP surface should be parallel or independent
    string orient_distrib_type;			//Define the GNP orientation type (random or specific) in a sample
    string size_distrib_type;			//Define the distribution type (uniform or normal) of the length (unit: micrometer) of GNP
    string thick_distrib_type;			//Define the distribution type (uniform or normal) of the thickness (unit: micrometer) of GNP
    double discr_step_length;                 //Define the step length (unit: micromether) for discretization of the GNP
    double ini_sita, ini_pha;			//Define initial GNP orientation for 'specific' type the spherical coordinates
    double len_min, len_max;            //Define the length range (min, max) for the width and length of the GNP surface
    double t_min, t_max;                //Define the thickness range (min,max) of the GNP
    double mass_ratio;                  //Define the CNT/GNP mass ratio
    double volume_fraction;             //Define the volume fraction of GNPs
    double real_volume;                 //Define the real volume of GNPs
    double weight_fraction;             //Define the weight fraction of GNPs
    double real_weight;                 //Define the real weight of GNPs
    double density;                     //Define the density of GNPs
};
//The parameters of nanotube agglomerates
struct Agglomerate_Geo{
    string keywords;
    int print_key;								//0 denotes "no print"; 1 denotes "only print the nanotubes in the ellipsoids"; 2 denotes "print the nanotubes in the ellipsolds and the surfaces of all ellipsoids".
    bool mark;
    double volf_clust;						//Define the volume fraction of nanotubes in clusters
    double vol_fra_criterion;			//Define the volume fraction of clusters in the sample
    double amin;								//Define the minimum value of long axis of a cluster ellipsoid
    double amax;							//Define the maximum value of long axis of a cluster ellipsoid
    double bmin;								//Define the minimum value of middle axis of a cluster ellipsoid
    double cmin;								//Define the minimum value of short axis of a cluster ellipsoid
    double growth_probability;		//Define the growth probability of nanotubes in a cluster
    double cnt_real_volume;			//Define the real volume of nanotubes in clusters
    vector<struct elliparam> ellips;  //Define the vector of ellipsoids for nanotube cluster zones
};
//The cutoff distances
struct Cutoff_dist{
    string keywords;
    bool mark;
    double van_der_Waals_dist;
    double tunneling_dist;
};
//The electrical parameters
struct Electric_para{
    string keywords;
    bool mark;
    double applied_voltage;			//Define the magnitude of the applied voltage
    double resistivity_CF;			//Define the resistivity value of the carbon fiber
    double sheet_resitance_GNP, resistivity_GNP_t, resistivity_GNP_surf; //Define the resistivity value of the GNP along the thicknes direction and along the surface
    double resistivity_matrix;      //Define the resistivity of the polymer matrix
    //Constants for tunneling
    double e_charge;
    //Constants using Li et al approach
    double e0_vacuum;
    double CNT_work_function;
    double K_polymer;
    //Constants using Hu et al approach
    double h_plank;
    double e_mass;
    double lambda_barrier;
};
struct Tecplot_flags{
    string keywords;
    bool mark;
    int generated_cnts;         //Flag to export generated CNTs
    int generated_gnps;         //Flag to export generated GNPs
    int clusters;               //Flag to export clusters as obtained from the Hoshen-Kopelman algorithm
    int percolated_clusters;    //Flag to export percolated clusters
    int backbone;               //Flag to export the backbone
    int triangulations;         //Flag to export triangulations
};
//---------------------------------------------------------------------------
class Input
{
public:
    //Data members
    struct App_name app_name;
    struct Simu_para simu_para;
    struct Geom_sample geom_sample;
    struct Nanotube_Geo nanotube_geo;
    struct Agglomerate_Geo agg_geo;
    struct Cutoff_dist cutoff_dist;
    struct Electric_para electric_para;
    struct GNP_Geo gnp_geo;
    struct Tecplot_flags tec360_flags;
    
    //Constructor
    Input(){};
    
    //Member functions
    int Data_Initialization();								//Initialize data
    void Warning_message(const string &str);
    void Warning_message_already_input(const string &str);
    int Read_Infile(ifstream &infile);				//Read data
    string Get_Line(ifstream &infile)const;		//Read the input data in a whole line (to skip over the comment line starting with a '%')
private:
    //Member functions
    int Read_application(struct App_name &app_name, ifstream &infile);
    int Read_simulation_parameters(struct Simu_para &simu_para, ifstream &infile);
    int Read_sample_geometry(struct Geom_sample &geom_sample, ifstream &infile);
    int Read_nanotube_geo_parameters(struct Nanotube_Geo &nanotube_geo, ifstream &infile);
    int Read_agg_geo_parameters(struct Agglomerate_Geo &agg_geo, ifstream &infile);
    int Read_cutoff_distances(struct Cutoff_dist &cutoff_dist, ifstream &infile);
    int Read_electrical_parameters(struct Electric_para &electric_para, ifstream &infile);
    int Read_gnp_geo_parameters(struct GNP_Geo &gnp_geo, ifstream &infile);
    int Read_tecplot_flags(struct Tecplot_flags &tec360_flags, ifstream &infile);
};
//---------------------------------------------------------------------------
#endif
//===========================================================================
