// Copyright (c) 2017-2018 Hartmut Kaiser
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <phylanx/config.hpp>
#include <phylanx/ir/node_data.hpp>
#include <phylanx/plugins/controls/while_operation.hpp>

#include <hpx/include/lcos.hpp>
#include <hpx/include/naming.hpp>
#include <hpx/include/util.hpp>
#include <hpx/throw_exception.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
namespace phylanx { namespace execution_tree { namespace primitives
{
    ///////////////////////////////////////////////////////////////////////////
    match_pattern_type const while_operation::match_data =
    {
        hpx::util::make_tuple("while",
            std::vector<std::string>{"while(_1, _2)"},
            &create_while_operation, &create_primitive<while_operation>,
            "cond, block\n"
            "Args:\n"
            "\n"
            "    cond (boolean expression): if it evaluates to True,\n"
            "                        execute the loop again.\n"
            "    block (statement) : code to execute as long as `cond`\n"
            "                        is true.\n"
            "\n"
            "Returns:\n"
            "\n"
            "  The value returned from the last iteration, `nil` otherwise."
            )
    };

    ///////////////////////////////////////////////////////////////////////////
    while_operation::while_operation(
            primitive_arguments_type&& operands,
            std::string const& name, std::string const& codename)
      : primitive_component_base(std::move(operands), name, codename)
    {}

    struct while_operation::iteration : std::enable_shared_from_this<iteration>
    {
        iteration(primitive_arguments_type const& args,
            std::shared_ptr<while_operation const> that)
          : that_(that), args_(args)
        {
            this->args_ = args;
            if (that_->operands_.size() != 2)
            {
                HPX_THROW_EXCEPTION(hpx::bad_parameter,
                    "phylanx::execution_tree::primitives::while_operation::"
                        "while_operation",
                    util::generate_error_message(
                        "the while_operation primitive requires exactly two "
                            "arguments",
                        that_->name_, that_->codename_));
            }

            if (!valid(that_->operands_[0]) || !valid(that_->operands_[1]))
            {
                HPX_THROW_EXCEPTION(hpx::bad_parameter,
                    "phylanx::execution_tree::primitives::while_operation::"
                        "while_operation",
                    util::generate_error_message(
                        "the while_operation primitive requires that the "
                            "arguments  by the operands array are valid",
                        that_->name_, that_->codename_));
            }
        }

        hpx::future<primitive_argument_type> loop()
        {
            // Evaluate condition of while statement
            auto this_ = this->shared_from_this();
            return literal_operand(
                that_->operands_[0], args_, that_->name_, that_->codename_)
                .then(hpx::launch::sync,
                    [this_](hpx::future<primitive_argument_type>&& cond)
                            -> hpx::future<primitive_argument_type> {
                    return this_->body(std::move(cond));
                });
        }

        hpx::future<primitive_argument_type> body(
            hpx::future<primitive_argument_type>&& cond)
        {
            if (extract_boolean_value_scalar(
                    cond.get(), that_->name_, that_->codename_))
            {
                // Evaluate body of while statement
                auto this_ = this->shared_from_this();
                return literal_operand(
                    that_->operands_[1], args_, that_->name_, that_->codename_)
                    .then(hpx::launch::sync,
                        [this_](hpx::future<primitive_argument_type>&&
                                    result) mutable
                        -> hpx::future<primitive_argument_type> {
                        this_->result_ = result.get();
                        return this_->loop();
                    });
            }

            return hpx::make_ready_future(std::move(result_));
        }

    private:
        primitive_arguments_type args_;
        primitive_argument_type result_;
        std::shared_ptr<while_operation const> that_;
    };

    // Start iteration over given while statement
    hpx::future<primitive_argument_type> while_operation::eval(
        primitive_arguments_type const& args) const
    {
        if (this->no_operands())
        {
            return std::make_shared<iteration>(noargs, shared_from_this())
                ->loop();
        }
        return std::make_shared<iteration>(args, shared_from_this())->loop();
    }
}}}
