#include "AeroControllerProto.hh"

using namespace boost::asio;
using namespace aero;
using namespace controller;

//////////////////////////////////////////////////
SEED485Controller::SEED485Controller(
    const std::string& _port, uint8_t _id) :
  ser_(io_), verbose_(true), id_(_id)
{
  if (_port == "") {
    std::cerr << "empty serial port name: entering debug mode...\n";
    return;
  }

  boost::system::error_code err;

  ser_.open(_port, err);
  usleep(1000 * 1000);

  if (err) {
    std::cerr << "could not open " << _port << "\n";
    return;
  }

  try {
    ser_.set_option(serial_port_base::baud_rate(1000000));
//     struct termios tio;
// #if ((BOOST_VERSION / 100 % 1000) > 50)
//     ::tcgetattr(ser_.lowest_layer().native_handle(), &tio);
//     ::cfsetospeed(&tio, 1000000);
//     ::cfsetispeed(&tio, 1000000);
//     ::tcsetattr(ser_.lowest_layer().native_handle(), TCSANOW, &tio);
// #else  // 12.04
//     ::tcgetattr(ser_.lowest_layer().native(), &tio);
//     ::cfsetospeed(&tio, 1000000);
//     ::cfsetispeed(&tio, 1000000);
//     ::tcsetattr(ser_.lowest_layer().native(), TCSANOW, &tio);
// #endif
  } catch (std::exception& e) {
    std::cerr << "baudrate: " << e.what() << "\n";
  }

  try {
    ser_.set_option(serial_port_base::character_size(
	serial_port_base::character_size(8)));
    ser_.set_option(serial_port_base::flow_control(
        serial_port_base::flow_control::none));
    ser_.set_option(serial_port_base::parity(
        serial_port_base::parity::none));
    ser_.set_option(serial_port_base::stop_bits(
        serial_port_base::stop_bits::one));
  } catch (std::exception& e) {
    std::cerr << e.what() << "\n";
  }
}

//////////////////////////////////////////////////
SEED485Controller::~SEED485Controller()
{
  if (ser_.is_open()) ser_.close();
}

//////////////////////////////////////////////////
void SEED485Controller::read(std::vector<uint8_t>& _read_data)
{
  _read_data.resize(RAW_DATA_LENGTH);

  if (ser_.is_open()) {
    boost::mutex::scoped_lock lock(mtx_);
    ser_.read_some(buffer(_read_data, RAW_DATA_LENGTH));
  }

  if (verbose_) {
    std::cout << "recv: ";
    for (size_t i = 0; i < _read_data.size(); ++i) {
      std::cout << std::setw(2)
                << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0')
                << static_cast<int32_t>(_read_data[i]);
    }
    std::cout << "\n";
  }
}

//////////////////////////////////////////////////
void SEED485Controller::send_command(
    uint8_t _cmd, uint16_t _time, std::vector<uint8_t>& _send_data)
{
  _send_data[0] = 0xFD;
  _send_data[1] = 0xDF;
  _send_data[2] = 0x40;
  _send_data[3] = _cmd;
  _send_data[4] = 0x00;

  //  time (2 bytes)
  _send_data[65] = static_cast<uint8_t>(0xff & (_time >> 8));
  _send_data[66] = static_cast<uint8_t>(0xff & _time);

  // check sum
  int32_t b_check_sum = 0;

  for (size_t i = 2; i < RAW_DATA_LENGTH - 1; ++i)
    b_check_sum += _send_data[i];
  _send_data[RAW_DATA_LENGTH - 1] =
    ~(reinterpret_cast<uint8_t*>(&b_check_sum)[0]);

  send_data(_send_data);
}

//////////////////////////////////////////////////
void SEED485Controller::flush()
{
  if (ser_.is_open()) {
    boost::mutex::scoped_lock lock(mtx_);
#if ((BOOST_VERSION / 100 % 1000) > 50)
    ::tcflush(ser_.lowest_layer().native_handle(), TCIOFLUSH);
#else  // 12.04
    ::tcflush(ser_.lowest_layer().native(), TCIOFLUSH);
#endif
  }
}

//////////////////////////////////////////////////
void SEED485Controller::send_data(std::vector<uint8_t>& _send_data)
{
  if (ser_.is_open()) {
    boost::mutex::scoped_lock lock(mtx_);
    ser_.write_some(buffer(_send_data));
  }

  if (verbose_) {
    std::cout << "send: ";
    for (size_t i = 0; i < _send_data.size(); ++i) {
      std::cout << std::uppercase << std::hex
		<< std::setw(2) << std::setfill('0')
		<< static_cast<int32_t>(_send_data[i]);
    }
    std::cout << "\n";
  }
}


//////////////////////////////////////////////////
AeroControllerProto::AeroControllerProto(const std::string& _port,
					 uint8_t _id) :
    seed_(_port, _id), verbose_(true)
{
}

//////////////////////////////////////////////////
AeroControllerProto::~AeroControllerProto()
{
}

//////////////////////////////////////////////////
void AeroControllerProto::servo_on()
{
  servo_command(1);
}

//////////////////////////////////////////////////
void AeroControllerProto::servo_off()
{
  servo_command(0);
}

//////////////////////////////////////////////////
void AeroControllerProto::servo_command(int16_t _d0)
{
  boost::mutex::scoped_lock lock(ctrl_mtx_);

  std::vector<int16_t> stroke_vector(stroke_joint_indices_.size(), _d0);

  std::vector<uint8_t> dat(RAW_DATA_LENGTH);

  stroke_to_raw_(stroke_vector, dat);

  seed_.send_command(CMD_MOTOR_SRV, 0, dat);
}

//////////////////////////////////////////////////
std::vector<int16_t> AeroControllerProto::get_reference_stroke_vector()
{
  return stroke_ref_vector_;
}

//////////////////////////////////////////////////
std::vector<int16_t> AeroControllerProto::get_actual_stroke_vector()
{
  return stroke_cur_vector_;
}

//////////////////////////////////////////////////
std::string AeroControllerProto::get_stroke_joint_name(size_t _idx)
{
  return stroke_joint_indices_[_idx].joint_name;
}

//////////////////////////////////////////////////
int AeroControllerProto::get_number_of_angle_joints()
{
  return angle_joint_indices_.size();
}

//////////////////////////////////////////////////
int32_t AeroControllerProto::get_ordered_angle_id(std::string _name)
{
  auto it = angle_joint_indices_.find(_name);
  if (it != angle_joint_indices_.end())
      return angle_joint_indices_[_name];
  return -1;
}

//////////////////////////////////////////////////
void AeroControllerProto::get_current(std::vector<int16_t>& _stroke_vector)
{
  get_command(CMD_GET_CUR, _stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::get_temperature(
    std::vector<int16_t>& _stroke_vector)
{
  get_command(CMD_GET_TMP, _stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::get_data(std::vector<int16_t>& _stroke_vector)
{
  std::vector<uint8_t> dat;
  dat.resize(RAW_DATA_LENGTH);

  seed_.read(dat);

  uint16_t header = decode_short_(&dat[0]);
  int16_t cmd;
  uint8_t* bvalue = reinterpret_cast<uint8_t*>(&cmd);
  bvalue[0] = dat[3];
  bvalue[1] = 0x00;

  if (header != 0xdffd) {
    seed_.flush();
    return;
  }

  if (cmd == 0x14 || cmd == 0x41 || cmd == 0x42 || cmd == 0x43
      || cmd == 0x44 || cmd == 0x45 || cmd == 0x52) {
    _stroke_vector.resize(stroke_joint_indices_.size());
    // raw to stroke
    for (size_t i = 0; i < stroke_joint_indices_.size(); ++i) {
      AJointIndex& aji = stroke_joint_indices_[i];
      // uint8_t -> uint16_t
      _stroke_vector[aji.stroke_index] =
        decode_short_(&dat[RAW_HEADER_OFFSET + aji.raw_index * 2]);
      // check value
      if (_stroke_vector[aji.stroke_index] > 0x7fff)
        _stroke_vector[aji.stroke_index] -= std::pow(2, 16);
      else if (_stroke_vector[aji.stroke_index] == 0x7fff)
        _stroke_vector[aji.stroke_index] = 0;
    }
  }

}

//////////////////////////////////////////////////
void AeroControllerProto::get_command(uint8_t _cmd,
                                      std::vector<int16_t>& _stroke_vector)
{
  boost::mutex::scoped_lock lock(ctrl_mtx_);

  std::vector<uint8_t> dat(RAW_DATA_LENGTH);
  seed_.send_command(_cmd, 0, dat);
  usleep(1000 * 20);  // wait
  get_data(_stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::set_position(
    std::vector<int16_t>& _stroke_vector, uint16_t _time)
{
  boost::mutex::scoped_lock lock(ctrl_mtx_);

  // for ROS
  for (size_t i = 0; i < _stroke_vector.size(); ++i)
    if (_stroke_vector[i] != 0x7fff)
      stroke_ref_vector_[i] = _stroke_vector[i];

  // for seed
  std::vector<uint8_t> dat(RAW_DATA_LENGTH);
  stroke_to_raw_(_stroke_vector, dat);
  seed_.send_command(CMD_MOVE_ABS, _time, dat);

  // for ROS
  usleep(0.02 * 1000 * 1000);
  get_data(stroke_cur_vector_);
}

//////////////////////////////////////////////////
void AeroControllerProto::set_max_current(
    std::vector<int16_t>& _stroke_vector)
{
  set_command(CMD_MOTOR_CUR, _stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::set_accel_rate(
    std::vector<int16_t>& _stroke_vector)
{
  set_command(CMD_MOTOR_ACC, _stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::set_motor_gain(
    std::vector<int16_t>& _stroke_vector)
{
  set_command(CMD_MOTOR_GAIN, _stroke_vector);
}

//////////////////////////////////////////////////
void AeroControllerProto::set_command(uint8_t _cmd,
                                      std::vector<int16_t>& _stroke_vector)
{
  boost::mutex::scoped_lock lock(ctrl_mtx_);

  std::vector<uint8_t> dat(RAW_DATA_LENGTH);
  stroke_to_raw_(_stroke_vector, dat);
  seed_.send_command(_cmd, 0, dat);
}

//////////////////////////////////////////////////
void AeroControllerProto::stroke_to_raw_(std::vector<int16_t>& _stroke,
                                         std::vector<uint8_t>& _raw)
{
  for (size_t i = 0; i < stroke_joint_indices_.size(); ++i) {
    AJointIndex& aji = stroke_joint_indices_[i];
    // uint16_t -> uint8_t
    encode_short_(_stroke[aji.stroke_index],
                  &_raw[RAW_HEADER_OFFSET + aji.raw_index * 2]);
  }
}

//////////////////////////////////////////////////
int16_t aero::controller::decode_short_(uint8_t* _raw)
{
  int16_t value;
  uint8_t* bvalue = reinterpret_cast<uint8_t*>(&value);
  bvalue[0] = _raw[1];
  bvalue[1] = _raw[0];
  return value;
}

//////////////////////////////////////////////////
void aero::controller::encode_short_(int16_t _value, uint8_t* _raw)
{
  uint8_t* bvalue = reinterpret_cast<uint8_t*>(&_value);
  _raw[0] = bvalue[1];
  _raw[1] = bvalue[0];
}
