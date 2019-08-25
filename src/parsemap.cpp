/**
 * parsemap.cpp - 
 * @author: Jonathan Beard
 * @version: Sun Mar 19 05:04:05 2017
 * 
 * Copyright 2017 Jonathan Beard
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cassert>
#include "parsemap.hpp"
#include "graphtools.hpp"

using namespace raft;

parsemap::parsemap() : submap()
{
   /** nothing really to do **/
}

parsemap::~parsemap()
{
    auto &container( all_kernels.acquire() );
    /** 
     * we have to get rid of the kernels now that they've been 
     * added so that the base destructor doesn't delete the 
     * internally allocated kernels 
     */
    container.clear();
    all_kernels.release();
    /** 
     * nothing else to clean up, rest...other virtual 
     * destructors.
     */
}

void
parsemap::push_state( raft::parse::state * const state )
{
    assert( state != nullptr ); 
    /**
     * TODO: finish me
     */
}

void
parsemap::parse_link( raft::kernel *src, 
                      raft::kernel *dst )
{
    parse_link_helper( src, dst );
    if( get_group_size() == 0 )
    {
        new_rhs_group();
    }
    updateKernels( src, dst );
    add_to_rhs_group( dst );
    /**
     * TODO: 
     * run function pointers for settings
     * pop state
     */
    return;
}



void 
raft::parsemap::parse_link_split( raft::kernel *src,
                                  raft::kernel *dst )
{
    /** a few sanity checks **/
    assert( src != nullptr );
    assert( dst != nullptr );
    assert( get_group_size() == 0 );
    /** link the first source -> destination **/
    const std::string enabled_port( dst->getEnabledPort() );
    updateKernels( src, dst );
    parse_link_helper( src, dst );
    if( get_group_size() == 0 )
    {
        new_rhs_group();
    }
    add_to_rhs_group( dst );
    /**
     * NOTE: following conditions should be met, src must 
     * have more than one output port, and multiple ports 
     * of the same type. The Destination must have at least
     * one active/enabled port. The destinations will be
     * added to a group. 
     */
     /** step 1, loop over ports **/
    auto &port( src->output );
    for( auto it( ++port.begin() /** inc past first **/ ); it != port.end(); ++it )
    {
        auto &kernel( (*src)[ it.name() ] );
        auto *cloned_kernel( dst->clone() );
        updateKernels( &kernel, cloned_kernel );
        parse_link_helper( &kernel, cloned_kernel );
        add_to_rhs_group( cloned_kernel );
    }
    return;
}


void 
raft::parsemap::parse_link_split_prepend( raft::kernel   *src /** this map is the implicit destination **/)
{
    assert( src != nullptr );
    /** take the first group_ptr_t in parse_head **/

    /**
     * rules: there's only one source, with N-output ports
     * 1) there must be an output port, along with 
     * one active input kernel for each of the destination
     * kernels
     */
    assert( parse_head.size() > 0 );
    /**
     * this is the head
     */
    auto &start( parse_head.front() );
    /**
     * check the number of kernels there
     */
    const auto dest_count( start->size() );
    assert( dest_count == 1 );

    /**
     * check the number of src output ports
     */
    const auto src_output_count( src->output.count() );
    if( src_output_count < 1 )
    {
        //TODO - throw proper exception 
        assert( false );
    }
    enum parse_case : int { UNKNOWN, 
                            EQUAL_OUTPUT_DEST, 
                            SELECTED_OUTPUT_DESTINATION };
    parse_case state = UNKNOWN;
    /**
     * The split semantics say that we should duplicate the 
     * chains past the first, so...take the extant kernels in 
     * the LHS of the graph, then attach them to the ports
     * from the output side.
     */
    if( src->getEnabledPortCount() == 0 )
    {
        state = EQUAL_OUTPUT_DEST;
    }
    else
    {
        state = SELECTED_OUTPUT_DESTINATION;
    }
    auto &destination_original( start->front() );
    /** 
     * we need to add group to the head to 
     * represent the source as the new head
     * so...make a head, insert, then add
     * source to that head.
     */
    switch( state )
    {
        case( EQUAL_OUTPUT_DEST ):
        {
            /** 
             * easy case, no ports selected, semantics
             * say we simply iterate over src output
             * ports, and duplicate from there. 
             * Note: when we duplicate the kernels from
             * the LHS to RHS, we don't need to add them
             * into the groups, they're not accessible
             * to the DSL save for the end points...
             * those we'll add in. 
             */
            auto &src_output_ports( src->output );
            auto index( 0 );
            for( auto it( src_output_ports.begin() ); it!= src_output_ports.end(); ++it )
            {   
                /** activate the port on the source kernel **/
                auto &kernel(       (*src)[ it.name() ]               );
                if( index == 0 )
                {
                    /**
                     * if here, then we've already an initialized 
                     * source and destination, and all we need to 
                     * do is link it and add it. 
                     */
                    updateKernels(      &kernel, destination_original       );
                    parse_link_helper(  &kernel, destination_original       );
                    // all of these are already in the LHS structure
                }
                else
                {
                    /**
                     * we need to clone the graph from the LHS (old LHS)
                     * to the end point, then place the end kernels returned
                     * from the clone to the RHS end of this parse graph, and put the 
                     * source kernels in the LHS (ones in the middle aren't accessible
                     * regardless, but adding them here will make them accessible
                     * through the graph.
                     */
                    //FIXME
                    auto *temporary_map( GraphTools::duplicateBetweenVertices( nullptr, /** temp **/
                                                                               nullptr ) );
                    
                    auto *cloned_kernel( destination_original->clone() );
                    updateKernels( &kernel, cloned_kernel );
                    parse_link_helper( &kernel, cloned_kernel );
                    add_to_lhs_group( cloned_kernel );
                }
                index++;
            }
        }
        break;
        case( SELECTED_OUTPUT_DESTINATION ):
        {
            //TODO - write this, needs to have a new port select
            //mechanism. 
        }
        break;
        default:
            assert( false );
    }
    
    /** 
     * for all states we need to add a new head to the
     * parse graph.
     */
    new_lhs_group();
    add_to_lhs_group( src );


    return;
}




void 
raft::parsemap::parse_link_join(  raft::kernel *src,
                                  raft::kernel *dst )
{
    UNUSED( src );
    UNUSED( dst );
    return;
}

void
raft::parsemap::parse_link_continue(    /** source is implicit **/ 
                                        raft::kernel   *dst )
{
    /** 
     * we're assuming that if we're calling this that it's with 
     * a LHS that is a parsemap, lets check that assumption
     */
    assert( get_group_size() > 0 );
    std::vector< group_ptr_t  > temp_groups;
    std::stack< raft::kernel* > kernel_construction_stack;
    assert( dst != nullptr );
    kernel_construction_stack.push( dst );
    /** do we have any groups **/
    while( get_group_size() != 0 )
    {
        /** do for each group **/
        auto group( pop_group() );
        /** make new temp group **/
        temp_groups.push_back( std::make_unique< group_t >() );
        
        /** front load kernel construction for all in group **/
        auto curr_avail_kernels( kernel_construction_stack.size() );
        const auto needed_kernels( group->size() );
        for( ; curr_avail_kernels != needed_kernels; curr_avail_kernels++ )
        {
            kernel_construction_stack.push( dst->clone() );
        }
        for( auto *src_ptr : (*group) )
        {
            auto *dst_ptr( kernel_construction_stack.top() );
            kernel_construction_stack.pop();
             updateKernels( src_ptr, dst_ptr );
            parse_link_helper( src_ptr, dst_ptr );
            /**
             * TODO: run function pointers for settings
             */
            temp_groups.back()->emplace_back( dst_ptr );
        }
    }
    /**
     * TODO: empty state container 
     */
    /** this should be empty here **/
    assert( get_group_size() == 0 );
    /** move smart pointers from temp_groups to main parse head **/
    parse_head = std::move( temp_groups );
    return;
}


void 
raft::parsemap::parse_link_helper( raft::kernel *src,
                                   raft::kernel *dst )
{
    assert( src != nullptr );
    assert( dst != nullptr );
    PortInfo *src_port_info( nullptr ), 
             *dst_port_info( nullptr );
    /** 
     * check for enabled ports
     * a[ "x" ], x is enabled, 
     * each should have one enabled port
     */
    const auto src_name( src->getEnabledPort() );
    if( src_name.length() > 0 )
    {
        try
        {
            src_port_info = &( src->output.getPortInfoFor( src_name ) );
        }
        catch( AmbiguousPortAssignmentException &ex )
        {
            /** impl in mapbase **/
            portNotFound( false, ex, src );
        }
    }
    /** else assume single port **/
    else
    {
        try
        {
            src_port_info = &( src->output.getPortInfo() );
        }
        catch( AmbiguousPortAssignmentException &ex )
        {
            portNotFound( true, ex, src );
        }
    }
    /** let exception catch first, then check null **/
    assert( src_port_info != nullptr );

    /**
     * DST side 
     */
    const auto dst_name( dst->getEnabledPort() );
    if( dst_name.length() > 0 )
    {
        try
        {
            dst_port_info = &( dst->input.getPortInfoFor( dst_name ) );
            /** FIXME: This needs to be a port not found exception **/
        }
        catch( AmbiguousPortAssignmentException &ex )
        {
            /** impl in mapbase **/
            portNotFound( false, ex, dst );
        }
    }
    /** else assume single port **/
    else
    {
        try
        {
            dst_port_info = &( dst->input.getPortInfo() );
        }
        catch( AmbiguousPortAssignmentException &ex )
        {
            portNotFound( true, ex, dst );
        }
    }
    /** let exception catch first, then check null **/
    assert( dst_port_info != nullptr );
    /** assume we have good ports at this point **/
    join( *src, src_port_info->my_name, *src_port_info,
          *dst, dst_port_info->my_name, *dst_port_info );
    return;
}

void 
raft::parsemap::new_rhs_group()
{
    parse_head.emplace_back( std::make_unique< group_t >() );
    return;
}

void
raft::parsemap::new_lhs_group()
{
    parse_head.emplace( parse_head.begin(), 
                        std::make_unique< group_t >() );
}

void 
raft::parsemap::add_to_rhs_group( raft::kernel * const k )
{
    assert( k != nullptr );
    assert( get_group_size() > 0 );
    parse_head.back()->emplace_back( k );
    return;
}

void 
raft::parsemap::add_to_lhs_group( raft::kernel * const k )
{
    assert( k != nullptr );
    assert( get_group_size() > 0 );
    parse_head.front()->emplace_back( k );
    return;
}



std::size_t
raft::parsemap::get_group_size()
{
    return( parse_head.size() );
}

parsemap::group_ptr_t
raft::parsemap::pop_group()
{
    if( parse_head.size() == 0 )
    {
        /** TODO throw exception **/
        assert( false );
    }
    auto temp( std::move( parse_head.back() ) );
    parse_head.pop_back();
    return( temp );
}

void
raft::parsemap::updateKernels( raft::kernel * const a,
                               raft::kernel * const b )
{
    /** TODO, need to return true src-dst **/ 
    /** 
     * if b is ever entered as a source, remove
     * from being destination. This does prevent
     * a user from building in feedback loops
     * in the ssame invocation, but not overall
     */
    if( source_kernels.size() == 0 )
    {
        source_kernels += a;
    }
    /** else if destination was used as a source, remove **/
    auto &src_container( source_kernels.unsafeAcquire() );
    auto src_ret_value( src_container.find( b ) );
    if( src_ret_value != src_container.cend() )
    {
        src_container.erase( src_ret_value );       
    }
    all_kernels += a;
    /**
     * FIXME, need a method to do this more simply
     */
#if 0
    else if( a cloned from real source )
    {
        enter
    }
#endif
    /** 
     * don't need thread safety here, TODO, build 
     * test case to try to break 
     */
    auto &dst_container( dst_kernels.unsafeAcquire() );
    //check to see if a is entered as a dst
    auto dst_ret_value( dst_container.find( a ) );   
    if( dst_ret_value != dst_container.cend() )
    {
        /** a appears as former destination, remove **/
        dst_container.erase( dst_ret_value );
    }
    dst_kernels += b;
    all_kernels += b;
    return;
}