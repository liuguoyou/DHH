#include <functional>
#include <utility>
#include <vector>
#include <fstream>

#include "caffe/layers/covlogm_layer.hpp"
#include "caffe/util/math_functions.hpp"
#define isNAN(x)	((x)!=(x))	
#define INF	(1.0/0.0)	// infinity
#define PINF	INF	// positive infinity
#define NINF	-INF	// negative infinity
#define isINF(x)	(((x)==PINF)||((x)==NINF))
#define isPINF(x)	((x)==PINF))
#define isNINF(x)	((x)==NINF))
namespace caffe {
// if gradients explosive occures, use clip_gradints in the solver file
template <typename Dtype>
void CovlogmLayer<Dtype>::LayerSetUp(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  epsilon_ = 0.001;
  iter_ = 0;
}
 
template <typename Dtype>
void CovlogmLayer<Dtype>::Reshape(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  const int num = bottom[1]->num();
  const int dim = bottom[0]->count()/bottom[0]->num();
  top[0]->Reshape(num,1,dim,dim);
  singular_.Reshape(1,1,1,dim);
  U_.Reshape(num,1,dim,dim); 
  Vt_.Reshape(num,1,dim,dim);
  tmp_.Reshape(1,1,dim,dim);
  tmp2_.Reshape(1,1,dim,dim);
  tmp3_.Reshape(1,1,dim,dim);
  S_.Reshape(num,1,dim,dim);
  logC_.Reshape(num,1,dim,dim);
  K_.Reshape(1,1,dim,dim);
  D_.Reshape(1,1,dim,dim);
  Unit_.Reshape(1,1,dim,dim);
  caffe_set(tmp_.count(),Dtype(1.0),tmp_.mutable_cpu_data());
  caffe_cpu_diag_op(dim,dim,tmp_.cpu_data(),Unit_.mutable_cpu_data(),0);
  /*Res_.ReshapeLike(*bottom[0]);
 //fstream output;
 //output.open("/home/data/qiaoshishi/tmp.txt",ios::out);
 //for(int i = 0; i < dim; i++)
 //{
 // for(int j = 0; j < dim; j++)
 // {
 //	  output<<Unit_.cpu_data()[i*dim+j]<<'\t';
 // }
 // output<<std::endl;
 //}
 //output.close();*/
}

template <typename Dtype>
void CovlogmLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  const Dtype* seidx_data = bottom[1]->cpu_data();
  const Dtype* Unit_data = Unit_.cpu_data();
  const int num = bottom[1]->num();
  const int dim = bottom[0]->count()/bottom[0]->num();
  iter_ = iter_ + 1;
  
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* logC_data = logC_.mutable_cpu_data();
  Dtype* S_data = S_.mutable_cpu_data();
  Dtype* sigular_data = singular_.mutable_cpu_data();
  Dtype* U_data = U_.mutable_cpu_data();
  Dtype* Vt_data = Vt_.mutable_cpu_data();
  Dtype* tmp_data = tmp_.mutable_cpu_data();
  //Dtype* tmp2_data = tmp2_.mutable_cpu_data();
  
  //fstream output1,output2;
  //output1.open("/home/qiaoshishi/data_in.txt",ios::out|ios::app);
  //output1<<"batch:"<<iter_<<std::endl;
  //output2.open("/home/qiaoshishi/data_s.txt",ios::out|ios::app);
  //output2<<"batch:"<<iter_<<std::endl; 
  // compute the logm of each covariance F'F+epsilon*I  
  for (int i = 0; i < num; i++){
	  int offset_top = top[0]->offset(i);
	  int start = seidx_data[i*2];
	  int end = seidx_data[i*2+1];
	  int set_size = end-start+1;
	  int offset_bottom_first = bottom[0]->offset(start);
	  // svd may change the input data
	  caffe_copy(set_size*dim, bottom_data+offset_bottom_first, tmp_data);
	  //caffe_scal(set_size*dim, Dtype(100.),tmp_data);
	  //output1<<"num:"<<i<<std::endl;
	  //LOG(INFO)<<tmp_data[0];
	  /* for(int m = 0; m < set_size; m++){
		  for(int n =0; n < dim; n++){
			  output1<<tmp_data[m*dim+n]<<'\t';
		  }
		  output1<<std::endl;
	  }*/
	  //output1<<tmp_data[0];
	  //output1<<std::endl; 
	  
	  caffe_cpu_gesvd(set_size, dim, tmp_data, sigular_data, U_data+offset_top, Vt_data+offset_top);
	  caffe_cpu_diag(set_size, dim, set_size, sigular_data, S_data+offset_top);
	  //output2<<"num:"<<i<<std::endl;
	  //for(int m = 0; m < set_size; m ++){		  
		//  output2<<sigular_data[m]<<'\t';
	  //}
	  //output2<<std::endl;
	  caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans, dim, dim, set_size,Dtype(1.),S_data+offset_top,S_data+offset_top,Dtype(0.),tmp_data);
      caffe_axpy(dim*dim, epsilon_, Unit_data, tmp_data);
	  caffe_cpu_diag_op(dim, dim, tmp_data, logC_data+offset_top, 2);
	  
	  caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,dim,dim,dim,Dtype(1.),Vt_data+offset_top,logC_data+offset_top,Dtype(0.),tmp_data);
	  caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,dim,dim,dim,Dtype(1.),tmp_data,Vt_data+offset_top,Dtype(0.),top_data+offset_top);
  }
  //output1.close();
  //output2.close(); 
  
}

template <typename Dtype>
void CovlogmLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
                                          const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[1]) {
    LOG(FATAL) << this->type()
               << " Layer cannot backpropagate to label inputs.";
  }	
  if (propagate_down[0]) {  
  const Dtype* top_diff = top[0]->cpu_diff();
  const Dtype* Unit_data = Unit_.cpu_data();
  const Dtype* U_data = U_.cpu_data();
  const Dtype* logC_data = logC_.cpu_data();
  const Dtype* S_data = S_.cpu_data();  
  const Dtype* Vt_data = Vt_.cpu_data();
  const Dtype* seidx_data = bottom[1]->cpu_data();
  Dtype* K_data = K_.mutable_cpu_data();
  Dtype* D_data = D_.mutable_cpu_data();
  Dtype* tmp_data = tmp_.mutable_cpu_data();
  Dtype* tmp2_data = tmp2_.mutable_cpu_data();
  Dtype* tmp3_data = tmp3_.mutable_cpu_data();
  
  Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
  caffe_set(bottom[0]->count(), Dtype(0.0), bottom_diff);
  caffe_set(singular_.count(), Dtype(1.0), singular_.mutable_cpu_data());
  const Dtype* sigular_data = singular_.cpu_data();
  
  const int num = bottom[1]->num();
  const int dim = bottom[0]->count()/bottom[0]->num();
  /*fstream output1, output2, output3, output4;
  output1.open("/home/qiaoshishi/A.txt",ios::out|ios::app);
  output2.open("/home/qiaoshishi/B.txt",ios::out|ios::app);
  output3.open("/home/qiaoshishi/C.txt",ios::out|ios::app);
  output4.open("/home/qiaoshishi/D.txt",ios::out|ios::app);*/
  /*output1<<"batch:"<<iter_<<std::endl;
  output2<<"batch:"<<iter_<<std::endl;
  output3<<"batch:"<<iter_<<std::endl;*/
  //output4<<"batch:"<<iter_<<std::endl;   
  	for(int clip_id = 0; clip_id < num; clip_id++)
   {
	   /*output1<<"num:"<<clip_id<<std::endl;
       output2<<"num:"<<clip_id<<std::endl;
       output3<<"num:"<<clip_id<<std::endl;*/
	   //output4<<"num:"<<clip_id<<std::endl;
       int offset_top = top[0]->offset(clip_id);   
       int start = seidx_data[clip_id*2];
	   int end = seidx_data[clip_id*2+1];
	   int set_size = end-start+1;
	   int offset_bottom_first = bottom[0]->offset(start);  
       
       // 2 times of sym of the top gradient
       caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,dim,dim,dim,Dtype(1.),top_diff+offset_top, Unit_data, Dtype(0.),tmp_data);
       caffe_add(dim*dim, top_diff+offset_top, tmp_data, tmp2_data);

       // gradient of L w.r.t V 
       caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasTrans,dim,dim,dim,Dtype(1.),tmp2_data,Vt_data+offset_top, Dtype(0.),tmp_data);
       caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,dim,dim,dim,Dtype(1.),tmp_data,logC_data+offset_top, Dtype(0.),tmp3_data); 	   
	   
	   //compute D
	   
	   //transpose aL/aV
	   caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,dim,dim,dim,Dtype(1.),tmp3_data, Unit_data, Dtype(0.),tmp_data);
	   
       caffe_cpu_diag(set_size, set_size, set_size,sigular_data,tmp3_data); 
	   
       // transpose S 	   
   	   caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,dim,set_size,set_size,Dtype(1.),S_data+offset_top, tmp3_data, Dtype(0.), D_data);
	
       caffe_cpu_diag_op(set_size,set_size,D_data,tmp3_data,1);// inverse Sm, reset 0 for the inverse of quite small singular value 
	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,set_size,Dtype(1.),tmp3_data,tmp_data, Dtype(0.),D_data);// Sm^-1 * (aL/aV1)^T
	   
	   // (aL/aV2)*(V2)^T
	   caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,dim,dim,(dim-set_size),Dtype(1.),tmp_data+(set_size)*dim,Vt_data+offset_top+(set_size)*dim,Dtype(0.),K_data);
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),Vt_data+offset_top,K_data,Dtype(0.),tmp_data);// (V1)^T * (aL/aV2)*(V2)^T
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,set_size,Dtype(1.),tmp3_data,tmp_data,Dtype(0.),K_data);
	   
	   // D 
	   caffe_axpy(set_size*dim,Dtype(-1.),K_data,D_data);
	   
	   
       // create K
       for(int i = 0; i < set_size; i++){
		   for(int j = i; j < set_size; j++){
			   if(i==j)
				   K_data[i*set_size+j] = 0;
			   else{
				   K_data[i*set_size+j] = 1.0/(S_data[offset_top+j*dim+j]*S_data[offset_top+j*dim+j] - S_data[offset_top+i*dim+i]*S_data[offset_top+i*dim+i]);
				   K_data[j*set_size+i] = -K_data[i*set_size+j];
				   if (isINF(K_data[i*set_size+j])||isNAN(K_data[i*set_size+j])){
					   K_data[i*set_size+j] = 0.;
					   K_data[j*set_size+i] = 0.;
                       //output4<<"qss\t";					   
				    }
			   }
		   }
	   }
	   
	   // D*V 
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasTrans,set_size,dim,dim,Dtype(1.0),D_data,Vt_data+offset_top,Dtype(0.),tmp_data);
	   
	   // D*V*S^T
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasTrans,set_size,set_size,dim,Dtype(1.),tmp_data,S_data+offset_top,Dtype(0.),tmp3_data);
	   
	   // hadamard product
	   caffe_mul(set_size*set_size,K_data,tmp3_data,tmp_data);
	   
	   // 2 times of sym
	   caffe_cpu_diag(set_size, set_size, set_size,sigular_data,tmp3_data);
	   caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans,set_size,set_size,set_size,Dtype(1.),tmp_data,tmp3_data,Dtype(0.),K_data);
	   caffe_add(set_size*set_size,tmp_data,K_data,tmp3_data);
	   
	   // the third term
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,set_size,set_size,Dtype(1.),U_data+offset_top,tmp3_data,Dtype(0.),tmp_data);	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,set_size,Dtype(1.),tmp_data,S_data+offset_top,Dtype(0.),tmp3_data);	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),tmp3_data,Vt_data+offset_top,Dtype(0.),K_data);
	   
	   
	   //aL/aS
	   caffe_cpu_gemm<Dtype>(CblasTrans,CblasNoTrans, dim, dim, set_size,Dtype(1.),S_data+offset_top,S_data+offset_top,Dtype(0.),tmp_data);
       caffe_axpy(dim*dim, epsilon_, Unit_data, tmp_data);
	   caffe_cpu_diag_op(dim,dim,tmp_data,tmp3_data,1);	   	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),S_data+offset_top,tmp3_data,Dtype(0.),tmp_data);	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),tmp_data,Vt_data+offset_top,Dtype(0.),tmp3_data);	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),tmp3_data,tmp2_data,Dtype(0.),tmp_data);	   
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasTrans,set_size,dim,dim,Dtype(1.),tmp_data,Vt_data+offset_top,Dtype(0.),tmp2_data);
	   
	   //second term
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasTrans,set_size,dim,dim,Dtype(1.),D_data,Vt_data+offset_top,Dtype(0.),tmp_data);	        
	   caffe_axpy(set_size*dim,Dtype(-1.),tmp_data,tmp2_data);
	   caffe_cpu_diag_op(set_size,dim,tmp2_data,tmp_data,0);       
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,set_size,Dtype(1.),U_data+offset_top,tmp_data,Dtype(0.),tmp3_data);
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,dim,Dtype(1.),tmp3_data,Vt_data+offset_top,Dtype(0.),tmp2_data);
	   
	   //first term 
	   caffe_cpu_gemm<Dtype>(CblasNoTrans,CblasNoTrans,set_size,dim,set_size,Dtype(1.),U_data+offset_top,D_data,Dtype(0.),tmp_data);
	   
	   // sum the three terms -> aL/aF
	   caffe_add(set_size*dim,tmp_data,tmp2_data,tmp3_data);
	   caffe_axpy(set_size*dim,Dtype(-1.),K_data,tmp3_data);
	   /*for(int m = 0;m < set_size; m++){
		   for(int k = 0; k < dim; k++){
			  output1<<tmp_data[m*dim+k]<<'\t';
			  output2<<tmp2_data[m*dim+k]<<'\t';
			  output3<<K_data[m*dim+k]<<'\t'; 
		   }
		   output1<<std::endl;
		   output2<<std::endl;
		   output3<<std::endl;
	   }
	   
	   output1<<std::endl; 
	   output2<<std::endl;
	   output3<<std::endl;*/
	   /*for(int m = 0;m < set_size; m++){
		   for(int k = 0; k < dim; k++){
			  output4<<tmp3_data[m*dim+k]<<'\t'; 
		   }
		   
		   output4<<std::endl;
	   }
	   output4<<std::endl;*/	   
	   
	   caffe_copy(set_size*dim,tmp3_data,bottom_diff+offset_bottom_first);
	   //caffe_scal(set_size*dim, Dtype(0.01), bottom_diff+offset_bottom_first);
   }
   //output1.close();
   //output2.close();
   //output3.close();
   //output4.close();
  }   
 
}

#ifdef CPU_ONLY
STUB_GPU(CovlogmLayer);
#endif

INSTANTIATE_CLASS(CovlogmLayer);
REGISTER_LAYER_CLASS(Covlogm);

}  // namespace caffe
