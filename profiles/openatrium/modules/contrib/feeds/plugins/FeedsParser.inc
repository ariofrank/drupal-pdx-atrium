<?php
// $Id: FeedsParser.inc,v 1.10 2010/02/09 17:20:18 alexb Exp $

/**
 * Abstract class, defines interface for parsers.
 */
abstract class FeedsParser extends FeedsPlugin {

  /**
   * Parse content fetched by fetcher.
   *
   * Extending classes must implement this method.
   *
   * @param $batch
   *   FeedsImportBatch returned by fetcher.
   * @param FeedsSource $source
   *   Source information.
   */
  public abstract function parse(FeedsImportBatch $batch, FeedsSource $source);

  /**
   * Clear all caches for results for given source.
   *
   * @param FeedsSource $source
   *   Source information for this expiry. Implementers can choose to only clear
   *   caches pertaining to this source.
   */
  public function clear(FeedsSource $source) {}

  /**
   * Declare the possible mapping sources that this parser produces.
   *
   * @return
   *   An array of mapping sources, or FALSE if the sources can be defined by
   *   typing a value in a text field.
   *
   *   Example:
   *   array(
   *     'title' => t('Title'),
   *     'created' => t('Published date'),
   *     'url' => t('Feed item URL'),
   *     'guid' => t('Feed item GUID'),
   *   )
   */
  public function getMappingSources() {
    return FALSE;
  }

  /**
   * Get an element identified by $element_key of the given item.
   * The element key corresponds to the values in the array returned by
   * FeedsParser::getMappingSources().
   */
  public function getSourceElement($item, $element_key) {
    return isset($item[$element_key]) ? $item[$element_key] : '';
  }
}

/**
 * Defines an element of a parsed result. Such an element can be a simple type,
 * a complex type (derived from FeedsElement) or an array of either.
 *
 * @see FeedsEnclosure
 */
class FeedsElement {
  // The standard value of this element. This value can contain be a simple type,
  // a FeedsElement or an array of either.
  protected $value;

  /**
   * Constructor.
   */
  public function __construct($value) {
    $this->value = $value;
  }

  /**
   * @return
   *   Standard value of this FeedsElement.
   */
  public function getValue() {
    return $this->value;
  }

  /**
   * @return
   *   A string representation of this element.
   */
  public function __toString() {
    if (is_array($this->value)) {
      return 'Array';
    }
    if (is_object($this->value)) {
      return 'Object';
    }
    return (string) $this->value;
  }
}

/**
 * Enclosure element, can be part of the result array.
 */
class FeedsEnclosure extends FeedsElement {
  protected $mime_type;
  protected $file;

  /**
   * Constructor, requires MIME type.
   */
  public function __construct($value, $mime_type) {
    parent::__construct($value);
    $this->mime_type = $mime_type;
  }

  /**
   * @return
   *   MIME type of return value of getValue().
   */
  public function getMIMEType() {
    return $this->mime_type;
  }

  /**
   * @return
   *   The content of the referenced resource.
   */
  public function getContent() {
    feeds_include_library('http_request.inc', 'http_request');
    $result = http_request_get($this->getValue());
    if ($result->code != 200) {
      throw new Exception(t('Download of @url failed with code !code.', array('@url' => $this->getValue(), '!code' => $result->code)));
    }
    return $result->data;
  }

  /**
   * @return
   *   The file path to the downloaded resource referenced by the enclosure.
   *   Downloads resource if not downloaded yet.
   *
   * @todo Get file extension from mime_type.
   * @todo This is not concurrency safe.
   */
  public function getFile() {
    if(!isset($this->file)) {
      $dest = file_destination(file_directory_temp() .'/'. get_class($this) .'-'. basename($this->getValue()), FILE_EXISTS_RENAME);
      if (ini_get('allow_url_fopen')) {
        $this->file = copy($this->getValue(), $dest) ? $dest : 0;
      }
      else {
        $this->file = file_save_data($this->getContent(), $dest);
      }
      if ($this->file === 0) {
        throw new Exception(t('Cannot write content to %dest', array('%dest' => $dest)));
      }
    }
    return $this->file;
  }
}

/**
 * Defines a date element of a parsed result (including ranges, repeat).
 */
class FeedsDateTimeElement extends FeedsElement {

  // Start date and end date.
  public $start;
  public $end;

  /**
   * Constructor.
   *
   * @param $start
   *   A FeedsDateTime object or a date as accepted by FeedsDateTime.
   * @param $end
   *   A FeedsDateTime object or a date as accepted by FeedsDateTime.
   * @param $tz
   *   A PHP DateTimeZone object.
   */
  public function __construct($start = NULL, $end = NULL, $tz = NULL) {
    $this->start = (!isset($start) || ($start instanceof FeedsDateTime)) ? $start : new FeedsDateTime($start, $tz);
    $this->end = (!isset($end) || ($end instanceof FeedsDateTime)) ? $end : new FeedsDateTime($end, $tz);
  }

  /**
   * Override FeedsElement::getValue().
   */
  public function getValue() {
    return $this->start;
  }

  /**
   * Implementation of toString magic php method.
   */
  public function __toString() {
    $val = $this->getValue();
    if ($val) {
      return $val->format('U');
    }
    return '';
  }

  /**
   * Merge this field with another. Most stuff goes down when merging the two
   * sub-dates.
   *
   * @see FeedsDateTime
   */
  public function merge(FeedsDateTimeElement $other) {
    $this2 = clone $this;
    if ($this->start && $other->start) {
      $this2->start = $this->start->merge($other->start);
    }
    elseif ($other->start) {
      $this2->start = clone $other->start;
    }
    elseif ($this->start) {
      $this2->start = clone $this->start;
    }

    if ($this->end && $other->end) {
      $this2->end = $this->end->merge($other->end);
    }
    elseif ($other->end) {
      $this2->end = clone $other->end;
    }
    elseif ($this->end) {
      $this2->end = clone $this->end;
    }
    return $this2;
  }

  /**
   * Helper method for buildDateField(). Build a FeedsDateTimeElement object
   * from a standard formatted node.
   */
  protected static function readDateField($node, $field_name) {
    $field = content_fields($field_name);
    $ret = new FeedsDateTimeElement();
    if (isset($node->{$field_name}[0]['date']) && $node->{$field_name}[0]['date'] instanceof FeedsDateTime) {
      $ret->start = $node->{$field_name}[0]['date'];
    }
    if (isset($node->{$field_name}[0]['date2']) && $node->{$field_name}[0]['date2'] instanceof FeedsDateTime) {
      $ret->end = $node->{$field_name}[0]['date2'];
    }
    return $ret;
  }

  /**
   * Build a node's date CCK field from our object.
   *
   * @param $node
   *   The node to build the date field on.
   * @param $field_name
   *   The name of the field to build.
   */
  public function buildDateField($node, $field_name) {
    $field = content_fields($field_name);
    $oldfield = FeedsDateTimeElement::readDateField($node, $field_name);
    // Merge with any preexisting objects on the field; we take precedence.
    $oldfield = $this->merge($oldfield);
    $use_start = $oldfield->start;
    $use_end = $oldfield->end;

    // Set timezone if not already in the FeedsDateTime object
    $to_tz = date_get_timezone($field['tz_handling'], date_default_timezone_name());
    $temp = new FeedsDateTime(NULL, new DateTimeZone($to_tz));

    $db_tz = '';
    if ($use_start) {
      $use_start = $use_start->merge($temp);
      if (!date_timezone_is_valid($use_start->getTimezone()->getName())) {
        $use_start->setTimezone(new DateTimeZone("UTC"));
      }
      $db_tz = date_get_timezone_db($field['tz_handling'], $use_start->getTimezone()->getName());
    }
    if ($use_end) {
      $use_end = $use_end->merge($temp);
      if (!date_timezone_is_valid($use_end->getTimezone()->getName())) {
        $use_end->setTimezone(new DateTimeZone("UTC"));
      }
      if (!$db_tz) {
        $db_tz = date_get_timezone_db($field['tz_handling'], $use_end->getTimezone()->getName());
      }
    }
    if (!$db_tz) {
      return;
    }

    $db_tz = new DateTimeZone($db_tz);
    if (!isset($node->{$field_name})) {
      $node->{$field_name} = array();
    }
    if ($use_start) {
      $node->{$field_name}[0]['timezone'] = $use_start->getTimezone()->getName();
      $node->{$field_name}[0]['offset'] = $use_start->getOffset();
      $use_start->setTimezone($db_tz);
      $node->{$field_name}[0]['date'] = $use_start;
      /**
       * @todo the date_type_format line could be simplified based upon a patch
       *   DO issue #259308 could affect this, follow up on at some point.
       *   Without this, all granularity info is lost.
       *   $use_start->format(date_type_format($field['type'], $use_start->granularity));
       */
      $node->{$field_name}[0]['value'] = $use_start->format(date_type_format($field['type']));
    }
    if ($use_end) {
      // Don't ever use end to set timezone (for now)
      $node->{$field_name}[0]['offset2'] = $use_end->getOffset();
      $use_end->setTimezone($db_tz);
      $node->{$field_name}[0]['date2'] = $use_end;
      $node->{$field_name}[0]['value2'] = $use_end->format(date_type_format($field['type']));
    }
  }
}

/**
 * Extend PHP DateTime class with granularity handling, merge functionality and
 * slightly more flexible initialization parameters.
 *
 * This class is a Drupal independent extension of the >= PHP 5.2 DateTime
 * class.
 *
 * @see FeedsDateTimeElement class
 */
class FeedsDateTime extends DateTime {
  public $granularity = array();
  protected static $allgranularity = array('year', 'month', 'day', 'hour', 'minute', 'second', 'zone');

  /**
   * Overridden constructor.
   *
   * @param $time
   *   time string, flexible format including timestamp.
   * @param $tz
   *   PHP DateTimeZone object, NULL allowed
   */
  public function __construct($time = '', $tz = NULL) {
    if (is_numeric($time)) {
      // Assume timestamp.
      $time = "@". $time;
    }
    // PHP < 5.3 doesn't like the GMT- notation for parsing timezones.
    $time = str_replace("GMT-", "-", $time);
    $time = str_replace("GMT+", "+", $time);
    parent::__construct($time, $tz ? $tz : new DateTimeZone("UTC"));
    $this->setGranularityFromTime($time, $tz);
    if (!preg_match('/[a-zA-Z]/', $this->getTimezone()->getName())) {
      // This tz was given as just an offset, which causes problems
      $this->setTimezone(new DateTimeZone("UTC"));
    }
  }

  /**
   * This function will keep this object's values by default.
   */
  public function merge(FeedsDateTime $other) {
    $other_tz = $other->getTimezone();
    $this_tz = $this->getTimezone();
    // Figure out which timezone to use for combination.
    $use_tz = ($this->hasGranularity('zone') || !$other->hasGranularity('zone')) ? $this_tz : $other_tz;

    $this2 = clone $this;
    $this2->setTimezone($use_tz);
    $other->setTimezone($use_tz);
    $val = $this2->toArray();
    $otherval = $other->toArray();
    foreach (self::$allgranularity as $g) {
      if ($other->hasGranularity($g) && !$this2->hasGranularity($g)) {
        // The other class has a property we don't; steal it.
        $this2->addGranularity($g);
        $val[$g] = $otherval[$g];
      }
    }
    $other->setTimezone($other_tz);

    $this2->setDate($val['year'], $val['month'], $val['day']);
    $this2->setTime($val['hour'], $val['minute'], $val['second']);
    return $this2;
  }

  /**
   * Overrides default DateTime function. Only changes output values if
   * actually had time granularity. This should be used as a "converter" for
   * output, to switch tzs.
   *
   * In order to set a timezone for a datetime that doesn't have such
   * granularity, merge() it with one that does.
   */
  public function setTimezone(DateTimeZone $tz, $force = FALSE) {
    // PHP 5.2.6 has a fatal error when setting a date's timezone to itself.
    // http://bugs.php.net/bug.php?id=45038
    if (version_compare(PHP_VERSION, '5.2.7', '<') && $tz == $this->getTimezone()) {
      $tz = new DateTimeZone($tz->getName());
    }

    if (!$this->hasTime() || !$this->hasGranularity('zone') || $force) {
      // this has no time or timezone granularity, so timezone doesn't mean much
      // We set the timezone using the method, which will change the day/hour, but then we switch back
      $arr = $this->toArray();
      parent::setTimezone($tz);
      $this->setDate($arr['year'], $arr['month'], $arr['day']);
      $this->setTime($arr['hour'], $arr['minute'], $arr['second']);
      return;
    }
    parent::setTimezone($tz);
  }

  /**
   * Safely adds a granularity entry to the array.
   */
  public function addGranularity($g) {
    $this->granularity[] = $g;
    $this->granularity = array_unique($this->granularity);
  }

  /**
   * Removes a granularity entry from the array.
   */
  public function removeGranularity($g) {
    if ($key = array_search($g, $this->granularity)) {
      unset($this->granularity[$key]);
    }
  }

  /**
   * Checks granularity array for a given entry.
   */
  public function hasGranularity($g) {
    return in_array($g, $this->granularity);
  }

  /**
   * Returns whether this object has time set. Used primarily for timezone
   * conversion and fomratting.
   *
   * @todo currently very simplistic, but effective, see usage
   */
  public function hasTime() {
    return $this->hasGranularity('hour');
  }

  /**
   * Protected function to find the granularity given by the arguments to the
   * constructor.
   */
  protected function setGranularityFromTime($time, $tz) {
    $this->granularity = array();
    $temp = date_parse($time);
    // This PHP method currently doesn't have resolution down to seconds, so if
    // there is some time, all will be set.
    foreach (self::$allgranularity AS $g) {
      if ((isset($temp[$g]) && is_numeric($temp[$g])) || ($g == 'zone' && (isset($temp['zone_type']) && $temp['zone_type'] > 0))) {
        $this->granularity[] = $g;
      }
    }
    if ($tz) {
      $this->addGranularity('zone');
    }
  }

  /**
   * Helper to return all standard date parts in an array.
   */
  protected function toArray() {
    return array('year' => $this->format('Y'), 'month' => $this->format('m'), 'day' => $this->format('d'), 'hour' => $this->format('H'), 'minute' => $this->format('i'), 'second' => $this->format('s'), 'zone' => $this->format('e'));
  }
}
