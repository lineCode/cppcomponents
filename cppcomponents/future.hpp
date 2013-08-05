#pragma once
#ifndef INCLUDE_GUARD_CPPCOMPONENTS_FUTURE_HPP_08_01_2013_
#define INCLUDE_GUARD_CPPCOMPONENTS_FUTURE_HPP_08_01_2013_

#include "events.hpp"

#include <future>
#include "implementation/uuid_combiner.hpp"
#include <cstdio>

namespace cppcomponents{



	namespace detail{
		template<typename F, typename W, typename R>
		struct helper
		{
			F f_;
			W w_;

			// By having the resulting future,
			// avoid having destructor block
			helper(F f, W w)
				: f_(f)
				, w_(w)
			{
			}


			R operator()()
			{
				f_.wait();
				return w_(f_);
			}
		};
		typedef uuid < 0x59de594e , 0xb773 , 0x4e65 , 0xbaaf , 0x104b58c32abe> ifuture_uuid_base_t;

	}

	template<typename F, typename W>
	auto then(F f, W w) -> std::shared_future<decltype(w(f))>
	{
		// Emulation of future.then
		// Can change to return f.then(w) when available
		typedef decltype(w(f)) T;
		std::shared_future < decltype(w(f))> ret = std::async(std::launch::async, detail::helper<F, W, decltype(w(f))>(f, w));
		return ret;
	}



	template < class T, class TUUID = combine_uuid<detail::ifuture_uuid_base_t,	typename uuid_of<T>::uuid_type>,
	class TUUIDDelegate = combine_uuid<detail::delegate_uuid, typename uuid_of<void>::uuid_type, detail::delegate_return_type_uuid,
		TUUID> >
	struct IFuture : public define_interface<TUUID>{
		typedef IFuture<T, TUUID, TUUIDDelegate> IFutureTemplate;
		typedef delegate<void(use<IFuture<T, TUUID, TUUIDDelegate>>), TUUIDDelegate> delegate_type;
		typedef T value_type;
		T Get();
		void SetCompleted(use<delegate_type>);

		typedef typename IFutureTemplate::base_interface_t base_interface_t;

		template<class B>
		struct Interface : public cross_compiler_interface::define_unknown_interface<B, TUUID>{
			cross_compiler_interface::cross_function < Interface, 0, value_type(), cross_compiler_interface::detail::dummy_function<value_type()>> Get;
			cross_compiler_interface::cross_function < Interface, 1, void (use<delegate_type>), cross_compiler_interface::detail::dummy_function<void (use<delegate_type>)>> SetCompleted;

			Interface() : Get(this), SetCompleted(this){}

		};




		template<class CppComponentInterfaceExtrasT> struct InterfaceExtras : IFutureTemplate::template InterfaceExtrasBase<CppComponentInterfaceExtrasT>{
			cppcomponents::write_only_property < CppComponentInterfaceExtrasT, decltype(Interface<CppComponentInterfaceExtrasT>::SetCompleted)> Completed;
	
			
			InterfaceExtras():Completed(this){}
		};
	};

	template<class T,class TUUID, class TUUIDDelegate>
	std::shared_future<T> to_future(use < IFuture < T,TUUID,TUUIDDelegate >> fut){
		auto shared_promise = std::make_shared < std::promise<T>>();
		auto f = [shared_promise](use < IFuture < T, TUUID, TUUIDDelegate >> result)mutable{
			
			try{
				shared_promise->set_value(result.Get());
				shared_promise = nullptr;
			}
			catch (std::exception&){
				shared_promise->set_exception(std::current_exception());
			}
		};
		fut.Completed = make_delegate < delegate<void(use < IFuture < T, TUUID, TUUIDDelegate >> result),TUUIDDelegate>>(f);
		return shared_promise->get_future();
	}
	template<class TIFuture, class T>
	use<TIFuture> make_ifuture(std::shared_future<T> f);

	namespace detail{

		template<class TIFuture, class TFuture>

		struct ifuture_implementation
			: public cross_compiler_interface::implement_unknown_interfaces <
			ifuture_implementation<TIFuture, TFuture >,
			TIFuture::template Interface 
			>
		{
			typedef TIFuture ifuture_t;
			typedef typename ifuture_t::value_type value_type;
			typedef typename ifuture_t::delegate_type delegate_type;
			TFuture f_;
			std::shared_future<void> resulting_;

			value_type get(){
				return f_.get();
			}
			void set_completed(use<delegate_type> i){
				auto func = [i](TFuture sfuture)mutable{

					i(make_ifuture<TIFuture>(sfuture));
				};
				resulting_ = then(f_, func);
			}


			ifuture_implementation(TFuture f) : f_(f){
				this->template get_implementation<ifuture_t::template Interface>()->Get.template set_mem_fn < ifuture_implementation,
					&ifuture_implementation::get>(this);
				this->template get_implementation<ifuture_t::template Interface>()->SetCompleted.template set_mem_fn < ifuture_implementation,
					&ifuture_implementation::set_completed>(this);
			}

			~ifuture_implementation(){

			}
		};
	}


	template<class TIFuture, class T>
	use<TIFuture> make_ifuture(std::shared_future<T> f){
		std::unique_ptr < detail::ifuture_implementation < TIFuture, std::shared_future<T >> > t(new  detail::ifuture_implementation < TIFuture, std::shared_future < T >> (f));
		cppcomponents::use<TIFuture> fut(t->template get_implementation<TIFuture::template Interface>()->get_use_interface(), false);
		t.release();
		return fut;
	}

};


#endif