// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Intel Corporation. All Rights Reserved.

#include "terminal-parser.h"

namespace librealsense
{

    using namespace std;

	terminal_parser::terminal_parser(const std::string& xml_full_file_path)
	{
		if (!xml_full_file_path.empty())
		{
			auto sts = parse_xml_from_file(xml_full_file_path, _cmd_xml);
			if (!sts)
			{
				cout << "Provided XML not found!\n";
				return;
				//TODO - Remi check this case - providing xml content instead of path sould solve this
			}

			update_format_type_to_lambda(_format_type_to_lambda);
			cout << "Commands XML file - " << xml_full_file_path << " was loaded successfully. Type commands by name (e.g.'gvd'`).\n";
		}
	}

    vector<uint8_t> terminal_parser::build_raw_command_data(const command_from_xml& command, const vector<string>& params)
    {
        if (params.size() > command.parameters.size() && !command.is_cmd_write_data)
            throw runtime_error("Input string was not in a correct format!");

        vector<parameter> vec_parameters;
        for (auto param_index = 0; param_index < params.size(); ++param_index)
        {
            auto is_there_write_data = param_index >= int(command.parameters.size());
            auto name = (is_there_write_data) ? "" : command.parameters[param_index].name;
            auto is_reverse_bytes = (is_there_write_data) ? false : command.parameters[param_index].is_reverse_bytes;
            auto is_decimal = (is_there_write_data) ? false : command.parameters[param_index].is_decimal;
            auto format_length = (is_there_write_data) ? -1 : command.parameters[param_index].format_length;
            vec_parameters.push_back(parameter(name, params[param_index], is_decimal, is_reverse_bytes, format_length));
        }

        vector<uint8_t> raw_data;
        encode_raw_data_command(command, vec_parameters, raw_data);
        return raw_data;
    }


	std::vector<uint8_t> terminal_parser::parse_command(debug_interface* device, const std::string& line)
	{
        using namespace std;
        vector<string> tokens;
        stringstream ss(line);
        string word;
        while (ss >> word)
        {
            stringstream converter;
            converter << hex << word;
            tokens.push_back(word);
        }

        if (tokens.empty())
            throw runtime_error("Wrong input!");

        auto command_str = tokens.front();
        auto it = _cmd_xml.commands.find(command_str);
        if (it == _cmd_xml.commands.end())
            throw runtime_error("Command not found!");

        auto command = it->second;
        vector<string> params;
        for (auto i = 1; i < tokens.size(); ++i)
            params.push_back(tokens[i]);

        auto raw_data = build_raw_command_data(command, params);

        for (auto b : raw_data)
        {
            cout << hex << fixed << setfill('0') << setw(2) << (int)b << " ";
        }
        cout << endl;

        auto result = device->send_receive_raw_data(raw_data);

        unsigned returned_opcode = *result.data();
        // check returned opcode
        if (command.op_code != returned_opcode)
        {
            stringstream msg;
            msg << "OpCodes do not match! Sent 0x" << hex << command.op_code << " but received 0x" << hex << (returned_opcode) << "!";
            throw runtime_error(msg.str());
        }

        if (command.is_read_command)
        {
            string data;
            decode_string_from_raw_data(command, _cmd_xml.custom_formatters, result.data(), result.size(), data, _format_type_to_lambda);
            //cout << endl << data << endl;
            vector<uint8_t> data_vec;
            data_vec.insert(data_vec.begin(), data.begin(), data.end());
            return data_vec;
        }
        else
        {
            cout << endl << "Done!" << endl;
            return result;
        }
		
	}

}